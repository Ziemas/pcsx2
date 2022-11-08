/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SpuCore.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "IopDma.h"
#include "IopHw.h"

namespace SPU
{
	std::array<Reg32, 2> SPUCore::m_IRQA = {{{0}, {0}}};
	std::array<SPUCore::AttrReg, 2> SPUCore::m_ATTR = {{{0}, {0}}};
	SPUCore::IrqStat SPUCore::m_IRQ = {0};

	AudioSample SPUCore::GenSample(AudioSample input)
	{
		AudioSample Dry(0, 0);
		AudioSample Wet(0, 0);

		MemOut(OutBuf::SINL, input.left);
		MemOut(OutBuf::SINR, input.right);

		if (m_share.KeyOn.full != 0 || m_share.KeyOff.full != 0)
		{
			for (auto& v : m_voices)
			{
				v.ProcessKonKoff();
			}

			m_share.KeyOff.full = 0;
			m_share.KeyOn.full = 0;
		}

		GSVector8i irqa[2]{
			GSVector8i(m_IRQA[0].full).broadcast32(),
			GSVector8i(m_IRQA[1].full).broadcast32()};
		GSVector8i mask(GSVector8i(~0x7).broadcast32());
		GSVector8i buf_addrs(GSVector8i(m_CurrentBuffer * BufSize + m_BufPos + m_Id * OutBufCoreOffset).broadcast32() + OutBufVec);

		for (int i = 0; i < 2; i++)
		{
			if (m_ATTR[i].IRQEnable)
			{
				auto res = m_share.vNAX.vec[0] == irqa[i];
				res |= m_share.vNAX.vec[1] == irqa[i];
				res |= m_share.vNAX.vec[2] == irqa[i];

				res |= (m_share.vNAX.vec[0] & mask) == irqa[i];
				res |= (m_share.vNAX.vec[1] & mask) == irqa[i];
				res |= (m_share.vNAX.vec[2] & mask) == irqa[i];

				res |= buf_addrs == irqa[i];

				if (!res.allfalse())
				{
					if (i == 0)
						m_IRQ.CauseC0 = true;
					if (i == 1)
						m_IRQ.CauseC1 = true;

					spu2Irq();
				}
			}
		}

		for (auto& v : m_voices)
		{
			v.DecodeSamples();
		}

		VoiceVec interp_out{};
		for (int i = 0; i < 24; i += 4)
		{
			GSVector8i dec(
				*(u64*)m_voices[i + 0].Get(),
				*(u64*)m_voices[i + 1].Get(),
				*(u64*)m_voices[i + 2].Get(),
				*(u64*)m_voices[i + 3].Get());
			GSVector8i interp(
				*(u64*)gaussianTable[(m_share.Counter.uarr[i + 0] & 0xff0) >> 4].data(),
				*(u64*)gaussianTable[(m_share.Counter.uarr[i + 1] & 0xff0) >> 4].data(),
				*(u64*)gaussianTable[(m_share.Counter.uarr[i + 2] & 0xff0) >> 4].data(),
				*(u64*)gaussianTable[(m_share.Counter.uarr[i + 3] & 0xff0) >> 4].data());

			dec = dec.mul16hrs(interp);
			dec = dec.adds16(dec.yyww());
			auto lo = dec.adds16(dec.yxxxl());
			auto hi = dec.adds16(dec.yxxxh());

			interp_out.arr[i + 0] = lo.I16[0];
			interp_out.arr[i + 1] = hi.I16[4];
			interp_out.arr[i + 2] = lo.I16[8];
			interp_out.arr[i + 3] = hi.I16[12];
		}

		for (auto& v : m_voices)
		{
			v.UpdateVolume();
		}

		GSVector8i pmod_mask(GSVector8i(0xffff).broadcast32());
		GSVector8i factors[2]{m_share.OUTX.vec[0], m_share.OUTX.vec[1]};
		GSVector8i steps[2]{m_share.Pitch.vec[0].andnot(m_vPMON.vec[0]), m_share.Pitch.vec[1].andnot(m_vPMON.vec[1])};

		for (int i = 0; i < 2; i++)
		{
			auto pitch_lo = GSVector8i::i16to32(GSVector4i::cast(m_share.Pitch.vec[i]));
			auto factor_lo = GSVector8i::u16to32(GSVector4i::cast(factors[i]));
			auto pitch_hi = GSVector8i::i16to32(GSVector4i::cast(m_share.Pitch.vec[i].cddd()));
			auto factor_hi = GSVector8i::u16to32(GSVector4i::cast(factors[i].cddd()));

			auto reslo = pitch_lo.mul32lo(factor_lo).sra32(15) & pmod_mask;
			auto reshi = pitch_hi.mul32lo(factor_hi).sra32(15) & pmod_mask;
			steps[i] |= reslo.pu32(reshi).acbd() & m_vPMON.vec[i];
		}

		GSVector8i step_limit(GSVector8i(0x3fff).broadcast16());
		steps[0] = m_share.Counter.vec[0].add16(steps[0].min_u16(step_limit));
		steps[1] = m_share.Counter.vec[1].add16(steps[1].min_u16(step_limit));

		GSVector8i counter_mask(GSVector8i(0xfff).broadcast16());
		m_share.Counter.vec[0] = steps[0] & counter_mask;
		m_share.Counter.vec[1] = steps[1] & counter_mask;

		m_share.SamplePos.vec[0] = m_share.SamplePos.vec[0].add16(steps[0].srl16(12));
		m_share.SamplePos.vec[1] = m_share.SamplePos.vec[1].add16(steps[1].srl16(12));

		// Load noise into all elements
		GSVector8i noise[2]{
			GSVector8i::load(m_Noise.Get()).broadcast16(),
			GSVector8i::load(m_Noise.Get()).broadcast16()};

		// & with NON to only keep it for voices with noise selected
		noise[0] = noise[0] & m_vNON.vec[0];
		noise[1] = noise[1] & m_vNON.vec[1];

		// Load voice output, exclude voices with active noise
		GSVector8i samples[2]{
			interp_out.vec[0].andnot(m_vNON.vec[0]),
			interp_out.vec[1].andnot(m_vNON.vec[1])};

		// mix in noise
		samples[0] |= noise[0];
		samples[1] |= noise[1];

		// Apply ADSR volume
		samples[0] = samples[0].mul16hrs(m_share.ENVX.vec[0]);
		samples[1] = samples[1].mul16hrs(m_share.ENVX.vec[1]);

		// Save to OUTX (needs to be kept for later due to pitch mod)
		// offset it so it lines up with the voice using the data
		GSVector8i::store<false>(&m_share.OUTX.arr[1], samples[0].add16(GSVector8i(0x8000).broadcast16()));
		GSVector8i::store<false>(&m_share.OUTX.arr[17], samples[1].add16(GSVector8i(0x8000).broadcast16()));

		MemOut(OutBuf::Voice1, samples[0].I16[1]);
		MemOut(OutBuf::Voice3, samples[0].I16[3]);

		// Split to streo and apply l/r volume
		GSVector8i left[2], right[2];
		left[0] = samples[0].mul16hrs(m_share.VOLL.vec[0]);
		left[1] = samples[1].mul16hrs(m_share.VOLL.vec[1]);
		right[0] = samples[0].mul16hrs(m_share.VOLR.vec[0]);
		right[1] = samples[1].mul16hrs(m_share.VOLR.vec[1]);

		GSVector8i vc_dry_l[2], vc_dry_r[2], vc_wet_l[2], vc_wet_r[2];
		vc_dry_l[0] = left[0] & m_vVMIXL.vec[0];
		vc_dry_l[1] = left[1] & m_vVMIXL.vec[1];
		vc_dry_r[0] = right[0] & m_vVMIXR.vec[0];
		vc_dry_r[1] = right[1] & m_vVMIXR.vec[1];

		vc_wet_l[0] = left[0] & m_vVMIXEL.vec[0];
		vc_wet_l[1] = left[1] & m_vVMIXEL.vec[1];
		vc_wet_r[0] = right[0] & m_vVMIXER.vec[0];
		vc_wet_r[1] = right[1] & m_vVMIXER.vec[1];

		AudioSample VoicesDry(hsum(vc_dry_l[0], vc_dry_l[1]), hsum(vc_dry_r[0], vc_dry_r[1]));
		AudioSample VoicesWet(hsum(vc_wet_l[0], vc_wet_l[1]), hsum(vc_wet_r[0], vc_wet_r[1]));

		MemOut(OutBuf::MemOutEL, VoicesWet.left);
		MemOut(OutBuf::MemOutER, VoicesWet.right);
		MemOut(OutBuf::MemOutL, VoicesDry.left);
		MemOut(OutBuf::MemOutR, VoicesDry.right);

		AudioSample In(MemIn());

		GSVector8i core;
		core.I16[SINL] = input.left;
		core.I16[SINR] = input.right;
		core.I16[SINEL] = input.left;
		core.I16[SINER] = input.right;

		core.I16[MINL] = In.left;
		core.I16[MINR] = In.right;
		core.I16[MINEL] = In.left;
		core.I16[MINER] = In.right;

		core.I16[MSNDL] = VoicesDry.left;
		core.I16[MSNDR] = VoicesDry.right;
		core.I16[MSNDEL] = VoicesWet.left;
		core.I16[MSNDER] = VoicesWet.right;

		core &= m_vMMIX;
		core = core.mul16hrs(m_VOL);

		Dry.Mix({core.I16[MSNDL], core.I16[MSNDR]});
		Wet.Mix({core.I16[MSNDEL], core.I16[MSNDER]});

		Dry.Mix({core.I16[SINL], core.I16[SINR]});
		Wet.Mix({core.I16[SINEL], core.I16[SINER]});

		Dry.Mix({core.I16[MINL], core.I16[MINR]});
		Wet.Mix({core.I16[MINEL], core.I16[MINER]});

		auto EOut = m_Reverb.Run(Wet);
		EOut.Volume(m_EVOL);

		AudioSample Out(0, 0);
		Out.Mix(Dry);
		Out.Mix(EOut);

		m_BufPos++;
		if (m_BufPos == 0x50)
		{
			m_CurBypassBuf = 1 - m_CurBypassBuf;
			if (AdmaActive() && m_SPDIFConf.Bypass)
				RunADMA();
		}

		if (m_BufPos == 0x100)
		{
			m_BufPos &= 0xFF;
			m_CurrentBuffer = 1 - m_CurrentBuffer;
			m_IRQ.BufferHalf = m_CurrentBuffer;
			m_CurBypassBuf = 1 - m_CurBypassBuf;

			if (AdmaActive())
				RunADMA();
		}

		m_Noise.Run();
		m_MVOL.Run();
		Out.Volume(m_MVOL);
		return Out;
	}

	void SPUCore::TestIrq(u32 address)
	{
		for (int i = 0; i < 2; i++)
		{
			if (m_IRQA[i].full == address && m_ATTR[i].IRQEnable)
			{
				if (i == 0)
					m_IRQ.CauseC0 = true;
				if (i == 1)
					m_IRQ.CauseC1 = true;

				spu2Irq();
			}
		}
	}

	void SPUCore::TestIrq(u32 start, u32 end)
	{
		for (int i = 0; i < 2; i++)
		{
			if (m_ATTR[i].IRQEnable)
			{
				if (m_IRQA[i].full >= start && m_IRQA[i].full <= end)
				{
					if (i == 0)
						m_IRQ.CauseC0 = true;
					if (i == 1)
						m_IRQ.CauseC1 = true;

					spu2Irq();
				}
			}
		}
	}

	void SPUCore::RunDma()
	{
		if (m_DmaSize <= 0)
		{
			m_Stat.DMABusy = false;
			m_Stat.DMARequest = true;

			return;
		}

		if (AdmaActive())
		{
			// Switch to right channel
			if (m_BufDmaCount == 8)
			{
				u32 displacement;

				if (m_SPDIFConf.Bypass)
					displacement = ((1 - m_CurBypassBuf) * BufSize) + (InBufOffset * m_Id);
				else
					displacement = ((1 - m_CurrentBuffer) * BufSize) + (InBufOffset * m_Id);

				m_InternalTSA = static_cast<u32>(InBuf::MeminR) + displacement;
			}

			// One L/R adma buffer pair splits up into 16 fifo-sized transfers
			// here is the logic for managing that
			if (m_BufDmaCount > 0)
			{
				m_BufDmaCount--;
			}
			else
			{
				// Shouldn't be correct but DQVIII hangs otherwise
				// need to think on this
				m_Stat.DMABusy = false;
				m_Stat.DMARequest = true;
				return;
			}
		}

		// TODO: This ADMA stuff is way crappy
		if (m_ATTR[m_Id].CurrentTransMode == TransferMode::DMAWrite || AdmaActive())
			memcpy(&m_RAM[m_InternalTSA], m_MADR, DmaFifoSize * 2);
		if (m_ATTR[m_Id].CurrentTransMode == TransferMode::DMARead)
			memcpy(m_MADR, &m_RAM[m_InternalTSA], DmaFifoSize * 2);

		TestIrq(m_InternalTSA, m_InternalTSA + DmaFifoSize - 1);

		m_InternalTSA += DmaFifoSize;
		m_MADR += DmaFifoSize;
		m_DmaSize -= DmaFifoSize;

		if (m_Id == 0)
			HW_DMA4_MADR += DmaFifoSize * 2;
		if (m_Id == 1)
			HW_DMA7_MADR += DmaFifoSize * 2;

		if (m_DmaSize <= 0)
		{
			if (m_Id == 0)
				spu2DMA4Irq();
			if (m_Id == 1)
				spu2DMA7Irq();
		}

		PSX_INT((IopEventId)(IopEvt_SPU0DMA + m_Id), 1024);
	}

	void SPUCore::WriteMem(u32 addr, u16 value)
	{
		m_RAM[addr] = value;
	}

	void SPUCore::RunADMA()
	{
		u32 displacement;

		if (m_SPDIFConf.Bypass)
			displacement = ((1 - m_CurBypassBuf) * BufSize) + (InBufOffset * m_Id);
		else
			displacement = ((1 - m_CurrentBuffer) * BufSize) + (InBufOffset * m_Id);

		m_BufDmaCount = 16;

		m_InternalTSA = static_cast<u32>(InBuf::MeminL) + displacement;

		m_Stat.DMABusy = true;
		m_Stat.DMARequest = false;

		RunDma();
	}

	void SPUCore::DmaWrite(u16* madr, u32 size)
	{
		//Console.WriteLn(ConsoleColors::Color_Cyan, "SPU[%d] Dma WRITE %d shorts to %06x irqa[%04x]", m_Id, size, m_InternalTSA, m_IRQA[m_Id]);
		if (AdmaActive())
		{
			m_Stat.DMABusy = true;
			m_Stat.DMARequest = false;

			m_DmaSize = size;
			m_MADR = madr;

			// Run right away if we still need data for the current buffers.
			if (m_BufDmaCount > 0)
				RunDma();

			return;
		}

		m_DmaSize = size;
		m_MADR = madr;
		RunDma();
	}

	void SPUCore::DmaRead(u16* madr, u32 size)
	{
		//Console.WriteLn(ConsoleColors::Color_Cyan, "SPU[%d] Dma READ %d shorts from %06x", m_Id, size, m_InternalTSA);
		if (AdmaActive())
			return;

		m_DmaSize = size;
		m_MADR = madr;
		RunDma();
	}

	void SPUCore::MemOut(SPUCore::OutBuf buffer, s16 value)
	{
		auto displacement = (m_CurrentBuffer * BufSize) + m_BufPos;
		auto address = static_cast<u32>(buffer) + (m_Id * OutBufCoreOffset);
		// Irq is tested together with voice irqs
		//TestIrq(address + displacement);
		WriteMem(address + displacement, value);
	}

	u16 SPUCore::Read(u32 addr)
	{
		if (addr < 0x180)
		{
			u32 id = addr >> 4;
			addr &= 0xF;
			return m_voices[id].Read(addr);
		}
		if (addr >= 0x1C0 && addr < 0x2E0)
		{
			addr -= 0x1C0;
			u32 id = addr / 0xC;
			addr %= 0xC;
			return m_voices[id].ReadAddr(addr);
		}
		switch (addr)
		{
			case 0x180:
				return m_share.PitchMod.lo.GetValue();
			case 0x182:
				return m_share.PitchMod.hi.GetValue();
			case 0x184:
				return m_share.NON.lo.GetValue();
			case 0x186:
				return m_share.NON.hi.GetValue();
			case 0x188:
				return m_VMIXL.lo.GetValue();
			case 0x18A:
				return m_VMIXL.hi.GetValue();
			case 0x18c:
				return m_VMIXEL.lo.GetValue();
			case 0x18e:
				return m_VMIXEL.hi.GetValue();
			case 0x190:
				return m_VMIXR.lo.GetValue();
			case 0x192:
				return m_VMIXR.hi.GetValue();
			case 0x194:
				return m_VMIXER.lo.GetValue();
			case 0x196:
				return m_VMIXER.hi.GetValue();
			case 0x198:
				return m_MMIX.bits;
			case 0x19A:
				return m_ATTR[m_Id].bits;
			case 0x19C:
				return m_IRQA[m_Id].hi.GetValue();
			case 0x19E:
				return m_IRQA[m_Id].lo.GetValue();
			case 0x1A0:
				return m_share.KeyOn.lo.GetValue();
			case 0x1A2:
				return m_share.KeyOn.hi.GetValue();
			case 0x1A4:
				return m_share.KeyOff.lo.GetValue();
			case 0x1A6:
				return m_share.KeyOff.hi.GetValue();
			case 0x1A8:
				return m_TSA.hi.GetValue();
			case 0x1AA:
				return m_TSA.lo.GetValue();
			case 0x1B0:
				return m_Adma.bits;
			case 0x2e0:
				return m_Reverb.m_ESA.hi.GetValue();
			case 0x2e2:
				return m_Reverb.m_ESA.lo.GetValue();
			case 0x33c:
				return m_Reverb.m_EEA.hi.GetValue();
			case 0x340:
				return m_share.ENDX.lo.GetValue();
			case 0x342:
				return m_share.ENDX.hi.GetValue();
			case 0x344:
				return m_Stat.bits;
			case 0x760:
				return m_MVOL.left.Get();
			case 0x762:
				return m_MVOL.right.Get();
			case 0x764:
				return m_EVOL.left;
			case 0x766:
				return m_EVOL.right;
			case 0x768:
				return m_AVOL.left;
			case 0x76A:
				return m_AVOL.right;
			case 0x76C:
				return m_BVOL.left;
			case 0x76E:
				return m_BVOL.right;
			case 0x774:
				return m_Reverb.vIIR;
			case 0x776:
				return m_Reverb.vCOMB1;
			case 0x778:
				return m_Reverb.vCOMB2;
			case 0x77A:
				return m_Reverb.vCOMB3;
			case 0x77C:
				return m_Reverb.vCOMB4;
			case 0x77E:
				return m_Reverb.vWALL;
			case 0x780:
				return m_Reverb.vAPF1;
			case 0x782:
				return m_Reverb.vAPF2;
			case 0x784:
				return m_Reverb.vIN[0];
			case 0x786:
				return m_Reverb.vIN[1];
			case 0x7C0:
				return m_SPDIFConf.bits;
			case 0x7C2:
				return m_IRQ.bits;
			case 0x7C6:
				return m_SPDIFMedia.hi.GetValue();
			case 0x7C8:
				return m_SPDIFMedia.lo.GetValue();
			default:
				Console.WriteLn("UNHANDLED SPU[%d] READ ---- <- %04x", m_Id, addr);
				pxAssertMsg(false, "Unhandled SPU Read");
				return 0;
		}
	}

	void SPUCore::Write(u32 addr, u16 value)
	{
		if (addr < 0x180)
		{
			u32 id = addr >> 4;
			addr &= 0xF;
			m_voices[id].Write(addr, value);
			return;
		}
		if (addr >= 0x1C0 && addr < 0x2E0)
		{
			addr -= 0x1C0;
			u32 id = addr / 0xC;
			addr %= 0xC;
			m_voices[id].WriteAddr(addr, value);
			return;
		}
		switch (addr)
		{
			case 0x180:
				ExpandVoiceBitfield(value & ~1, m_share.PitchMod, m_vPMON.vec[0], false);
				break;
			case 0x182:
				ExpandVoiceBitfield(value, m_share.PitchMod, m_vPMON.vec[1], true);
				break;
			case 0x184:
				ExpandVoiceBitfield(value, m_share.NON, m_vNON.vec[0], false);
				break;
			case 0x186:
				ExpandVoiceBitfield(value, m_share.NON, m_vNON.vec[1], true);
				break;
			case 0x188:
				ExpandVoiceBitfield(value, m_VMIXL, m_vVMIXL.vec[0], false);
				break;
			case 0x18A:
				ExpandVoiceBitfield(value, m_VMIXL, m_vVMIXL.vec[1], true);
				break;
			case 0x18C:
				ExpandVoiceBitfield(value, m_VMIXEL, m_vVMIXEL.vec[0], false);
				break;
			case 0x18E:
				ExpandVoiceBitfield(value, m_VMIXEL, m_vVMIXEL.vec[1], true);
				break;
			case 0x190:
				ExpandVoiceBitfield(value, m_VMIXR, m_vVMIXR.vec[0], false);
				break;
			case 0x192:
				ExpandVoiceBitfield(value, m_VMIXR, m_vVMIXR.vec[1], true);
				break;
			case 0x194:
				ExpandVoiceBitfield(value, m_VMIXER, m_vVMIXER.vec[0], false);
				break;
			case 0x196:
				ExpandVoiceBitfield(value, m_VMIXER, m_vVMIXER.vec[1], true);
				break;
			case 0x198:
				for (int i = 0; i < 16; i++)
				{
					m_vMMIX.U16[i] = GET_BIT(i, value) ? 0xffff : 0;
				}
				m_MMIX.bits = value;
				break;
			case 0x19A:
				m_ATTR[m_Id].bits = value;
				if (!m_ATTR[m_Id].IRQEnable)
				{
					if (m_Id == 0)
						m_IRQ.CauseC0 = false;
					if (m_Id == 1)
						m_IRQ.CauseC1 = false;
				}
				m_Reverb.m_Enable = m_ATTR[m_Id].EffectEnable;
				m_Noise.SetClock(m_ATTR[m_Id].NoiseClock.GetValue());
				break;
			case 0x19C:
				m_IRQA[m_Id].hi = value;
				break;
			case 0x19E:
				m_IRQA[m_Id].lo = value;
				break;
			case 0x1B0:
				// Prevent leaving the ADMA machinery in dumb states
				if (value == 0)
					m_BufDmaCount = 0;
				m_Adma.bits = value;
				break;
			case 0x1A0:
				m_share.KeyOn.lo.SetValue(value);
				break;
			case 0x1A2:
				m_share.KeyOn.hi.SetValue(value);
				break;
			case 0x1A4:
				m_share.KeyOff.lo.SetValue(value);
				break;
			case 0x1A6:
				m_share.KeyOff.hi.SetValue(value);
				break;
			case 0x1A8:
				m_TSA.hi = value & 0xF;
				m_InternalTSA = m_TSA.full;
				break;
			case 0x1AA:
				m_TSA.lo = value;
				m_InternalTSA = m_TSA.full;
				break;
			case 0x1AC:
				// TODO: FIFO
				WriteMem(m_InternalTSA, value);
				++m_InternalTSA &= 0xFFFFF;
				break;
			case 0x2E0:
				m_Reverb.m_pos = 0;
				m_Reverb.m_ESA.hi = value & 0xF;
				break;
			case 0x2E2:
				m_Reverb.m_pos = 0;
				m_Reverb.m_ESA.lo = value;
				break;
			case 0x2E4:
				m_Reverb.dAPF[0].hi = value;
				break;
			case 0x2E6:
				m_Reverb.dAPF[0].lo = value;
				break;
			case 0x2E8:
				m_Reverb.dAPF[1].hi = value;
				break;
			case 0x2EA:
				m_Reverb.dAPF[1].lo = value;
				break;
			case 0x2EC:
				m_Reverb.mSAME[0].hi = value;
				break;
			case 0x2EE:
				m_Reverb.mSAME[0].lo = value;
				break;
			case 0x2F0:
				m_Reverb.mSAME[1].hi = value;
				break;
			case 0x2F2:
				m_Reverb.mSAME[1].lo = value;
				break;
			case 0x2F4:
				m_Reverb.mCOMB1[0].hi = value;
				break;
			case 0x2F6:
				m_Reverb.mCOMB1[0].lo = value;
				break;
			case 0x2F8:
				m_Reverb.mCOMB1[1].hi = value;
				break;
			case 0x2FA:
				m_Reverb.mCOMB1[1].lo = value;
				break;
			case 0x2FC:
				m_Reverb.mCOMB2[0].hi = value;
				break;
			case 0x2FE:
				m_Reverb.mCOMB2[0].lo = value;
				break;
			case 0x300:
				m_Reverb.mCOMB2[1].hi = value;
				break;
			case 0x302:
				m_Reverb.mCOMB2[1].lo = value;
				break;
			case 0x304:
				m_Reverb.dSAME[0].hi = value;
				break;
			case 0x306:
				m_Reverb.dSAME[0].lo = value;
				break;
			case 0x308:
				m_Reverb.dSAME[1].hi = value;
				break;
			case 0x30A:
				m_Reverb.dSAME[1].lo = value;
				break;
			case 0x30C:
				m_Reverb.mDIFF[0].hi = value;
				break;
			case 0x30E:
				m_Reverb.mDIFF[0].lo = value;
				break;
			case 0x310:
				m_Reverb.mDIFF[1].hi = value;
				break;
			case 0x312:
				m_Reverb.mDIFF[1].lo = value;
				break;
			case 0x314:
				m_Reverb.mCOMB3[0].hi = value;
				break;
			case 0x316:
				m_Reverb.mCOMB3[0].lo = value;
				break;
			case 0x318:
				m_Reverb.mCOMB3[1].hi = value;
				break;
			case 0x31A:
				m_Reverb.mCOMB3[1].lo = value;
				break;
			case 0x31C:
				m_Reverb.mCOMB4[0].hi = value;
				break;
			case 0x31E:
				m_Reverb.mCOMB4[0].lo = value;
				break;
			case 0x320:
				m_Reverb.mCOMB4[1].hi = value;
				break;
			case 0x322:
				m_Reverb.mCOMB4[1].lo = value;
				break;
			case 0x324:
				m_Reverb.dDIFF[0].hi = value;
				break;
			case 0x326:
				m_Reverb.dDIFF[0].lo = value;
				break;
			case 0x328:
				m_Reverb.dDIFF[1].hi = value;
				break;
			case 0x32A:
				m_Reverb.dDIFF[1].lo = value;
				break;
			case 0x32C:
				m_Reverb.mAPF1[0].hi = value;
				break;
			case 0x32E:
				m_Reverb.mAPF1[0].lo = value;
				break;
			case 0x330:
				m_Reverb.mAPF1[1].hi = value;
				break;
			case 0x332:
				m_Reverb.mAPF1[1].lo = value;
				break;
			case 0x334:
				m_Reverb.mAPF2[0].hi = value;
				break;
			case 0x336:
				m_Reverb.mAPF2[0].lo = value;
				break;
			case 0x338:
				m_Reverb.mAPF2[1].hi = value;
				break;
			case 0x33A:
				m_Reverb.mAPF2[1].lo = value;
				break;
			case 0x33C:
				m_Reverb.m_EEA.hi = value & 0xF;
				m_Reverb.m_EEA.lo = 0xFFFF;
				break;
			case 0x340:
				m_share.ENDX.lo.SetValue(value);
				break;
			case 0x342:
				m_share.ENDX.hi.SetValue(value);
				break;
			//case 0x344:
			//	// SPU Status R/O
			//	break;
			case 0x760:
				m_MVOL.left.Set(value);
				break;
			case 0x762:
				m_MVOL.right.Set(value);
				break;
			case 0x764:
				m_EVOL.left = static_cast<s16>(value);
				break;
			case 0x766:
				m_EVOL.right = static_cast<s16>(value);
				break;
			case 0x768:
				m_AVOL.left = static_cast<s16>(value);
				m_VOL.I16[SINL] = m_AVOL.left;
				m_VOL.I16[SINEL] = m_AVOL.left;
				break;
			case 0x76A:
				m_AVOL.right = static_cast<s16>(value);
				m_VOL.I16[SINR] = m_AVOL.right;
				m_VOL.I16[SINEL] = m_AVOL.right;
				break;
			case 0x76C:
				m_BVOL.left = static_cast<s16>(value);
				m_VOL.I16[MINL] = m_BVOL.left;
				m_VOL.I16[MINEL] = m_BVOL.left;
				break;
			case 0x76E:
				m_BVOL.right = static_cast<s16>(value);
				m_VOL.I16[MINR] = m_BVOL.right;
				m_VOL.I16[MINER] = m_BVOL.right;
				break;
			case 0x774:
				m_Reverb.vIIR = static_cast<int16_t>(value);
				break;
			case 0x776:
				m_Reverb.vCOMB1 = static_cast<int16_t>(value);
				break;
			case 0x778:
				m_Reverb.vCOMB2 = static_cast<int16_t>(value);
				break;
			case 0x77A:
				m_Reverb.vCOMB3 = static_cast<int16_t>(value);
				break;
			case 0x77C:
				m_Reverb.vCOMB4 = static_cast<int16_t>(value);
				break;
			case 0x77E:
				m_Reverb.vWALL = static_cast<int16_t>(value);
				break;
			case 0x780:
				m_Reverb.vAPF1 = static_cast<int16_t>(value);
				break;
			case 0x782:
				m_Reverb.vAPF2 = static_cast<int16_t>(value);
				break;
			case 0x784:
				m_Reverb.vIN[0] = static_cast<int16_t>(value);
				break;
			case 0x786:
				m_Reverb.vIN[1] = static_cast<int16_t>(value);
				break;
			case 0x7C0:
				m_SPDIFConf.bits = value;
				break;
			case 0x7C6:
				m_SPDIFMedia.hi.SetValue(value);
				break;
			case 0x7C8:
				m_SPDIFMedia.lo.SetValue(value);
				break;
			case 0x7CA:
				// Unk, libsd init writes 8 here
				break;
			default:
				Console.WriteLn("UNHANDLED SPU[%d] WRITE %04x -> %04x", m_Id, value, addr);
				pxAssertMsg(false, "Unhandled SPU Write");
		}
	}

	void SPUCore::Reset()
	{
		Console.WriteLn("SPU[%d] Reset", m_Id);
		m_SPDIFConf.bits = 0;
		m_SPDIFMedia.bits = 0;
		m_Stat.bits = 0;
		m_Adma.bits = 0;
		m_MADR = nullptr;
		m_DmaSize = 0;
		m_BufPos = 0;
		m_CurrentBuffer = 0;
		m_BufDmaCount = 0;
		m_TSA.full = 0;
		m_AVOL = {0};
		m_BVOL = {0};
		m_EVOL = {0};
		m_MMIX.bits = 0;
		m_VMIXL.full = 0;
		m_VMIXR.full = 0;
		m_VMIXEL.full = 0;
		m_VMIXER.full = 0;
		m_IRQA[m_Id].full = 0x800;
		m_ATTR[m_Id].bits = 0;
		m_IRQ.bits = 0;
		m_Reverb.Reset();
		for (auto& v : m_voices)
			v.Reset();
		m_Noise.Reset();
	}
} // namespace SPU
