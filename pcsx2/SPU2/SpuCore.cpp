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

		// TODO this is bit ugly isn't it
		// tempting to do the union thing spu2x does
		u32 vDryL = m_VMIXL.full;
		u32 vDryR = m_VMIXR.full;
		u32 vWetL = m_VMIXEL.full;
		u32 vWetR = m_VMIXER.full;
		for (auto& v : m_voices)
		{
			auto sample = v.GenSample();
			Dry.Mix(sample, vDryL & 1, vDryR & 1);
			Wet.Mix(sample, vWetL & 1, vWetR & 1);

			vDryL >>= 1;
			vDryR >>= 1;
			vWetL >>= 1;
			vWetR >>= 1;
		}

		input.Volume(m_AVOL);
		Dry.Mix(input, m_MMIX.SinL, m_MMIX.SinR);
		Wet.Mix(input, m_MMIX.SinWetL, m_MMIX.SinWetR);

		MemOut(OutBuf::MemOutL, Dry.left);
		MemOut(OutBuf::MemOutR, Dry.right);

		AudioSample MemIn(0, 0);

		if (AdmaActive())
		{
			auto displacement = (m_CurrentBuffer * BufSize) + m_BufPos + (m_Id * InBufOffset);
			auto laddress = static_cast<u32>(InBuf::MeminL) + displacement;
			auto raddress = static_cast<u32>(InBuf::MeminR) + displacement;
			MemIn.left = static_cast<s16>(m_RAM[laddress]);
			MemIn.right = static_cast<s16>(m_RAM[raddress]);
			MemIn.Volume(m_BVOL);
		}

		Dry.Mix(MemIn, m_MMIX.MeminL, m_MMIX.MeminR);
		Wet.Mix(MemIn, m_MMIX.MeminWetL, m_MMIX.MeminWetR);

		auto EOut = m_Reverb.Run(Wet);
		EOut.Volume(m_EVOL);

		MemOut(OutBuf::MemOutEL, EOut.left);
		MemOut(OutBuf::MemOutER, EOut.right);

		AudioSample Out(0, 0);
		Out.Mix(Dry, m_MMIX.VoiceL, m_MMIX.VoiceR);
		Out.Mix(EOut, m_MMIX.VoiceWetL, m_MMIX.VoiceWetR);

		m_BufPos++;
		if (m_BufPos == 0x100)
		{
			m_BufPos &= 0xFF;
			m_CurrentBuffer = 1 - m_CurrentBuffer;
			m_IRQ.BufferHalf = m_CurrentBuffer;

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
				auto displacement = ((1 - m_CurrentBuffer) * BufSize) + (InBufOffset * m_Id);
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

		// Exclude initial, include final
		TestIrq(m_InternalTSA + 1, m_InternalTSA + DmaFifoSize + 1);

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
		m_BufDmaCount = 16;
		auto displacement = ((1 - m_CurrentBuffer) * BufSize) + (InBufOffset * m_Id);
		m_InternalTSA = static_cast<u32>(InBuf::MeminL) + displacement;

		m_Stat.DMABusy = true;
		m_Stat.DMARequest = false;

		RunDma();
	}

	void SPUCore::DmaWrite(u16* madr, u32 size)
	{
		//Console.WriteLn(ConsoleColors::Color_Cyan, "SPU[%d] Dma WRITE %d shorts to %06x", m_Id, size, m_InternalTSA);
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
		TestIrq(address + displacement);
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
			{
				u32 ret = 0;
				for (int i = 0; i < 24; i++)
				{
					if (m_voices[i].m_PitchMod)
						SET_BIT(ret, i);
				}
				return GET_LOW(ret);
			}
			case 0x182:
			{
				u32 ret = 0;
				for (int i = 0; i < 24; i++)
				{
					if (m_voices[i].m_PitchMod)
						SET_BIT(ret, i);
				}
				return GET_HIGH(ret);
			}
			case 0x184:
			{
				u32 ret = 0;
				for (int i = 0; i < 24; i++)
				{
					if (m_voices[i].m_Noise)
						SET_BIT(ret, i);
				}
				return GET_LOW(ret);
			}
			case 0x186:
			{
				u32 ret = 0;
				for (int i = 0; i < 24; i++)
				{
					if (m_voices[i].m_Noise)
						SET_BIT(ret, i);
				}
				return GET_HIGH(ret);
			}
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
			{
				u32 ret = 0;
				for (int i = 0; i < 24; i++)
				{
					if (m_voices[i].m_KeyOn)
						SET_BIT(ret, i);
				}
				return GET_LOW(ret);
			}
			case 0x1A2:
			{
				u32 ret = 0;
				for (int i = 0; i < 24; i++)
				{
					if (m_voices[i].m_KeyOn)
						SET_BIT(ret, i);
				}
				return GET_HIGH(ret);
			}
			case 0x1A4:
			{
				u32 ret = 0;
				for (int i = 0; i < 24; i++)
				{
					if (m_voices[i].m_KeyOff)
						SET_BIT(ret, i);
				}
				return GET_LOW(ret);
			}
			case 0x1A6:
			{
				u32 ret = 0;
				for (int i = 0; i < 24; i++)
				{
					if (m_voices[i].m_KeyOff)
						SET_BIT(ret, i);
				}
				return GET_HIGH(ret);
			}
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
			{
				u32 ret = 0;
				for (int i = 0; i < 24; i++)
				{
					if (m_voices[i].m_KeyOff)
						SET_BIT(ret, i);
				}
				return GET_LOW(ret);
			}
			case 0x342:
			{
				u32 ret = 0;
				for (int i = 0; i < 24; i++)
				{
					if (m_voices[i].m_KeyOff)
						SET_BIT(ret, i);
				}
				return GET_HIGH(ret);
			}
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
			case 0x7C2:
				return m_IRQ.bits;
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
				for (int i = 0; i < 16; i++)
				{
					m_voices[i].m_PitchMod = GET_BIT(i, value);
				}
				break;
			case 0x182:
				for (int i = 0; i < 8; i++)
				{
					m_voices[i + 16].m_PitchMod = GET_BIT(i, value);
				}
				break;
			case 0x184:
				for (int i = 0; i < 16; i++)
				{
					m_voices[i].m_Noise = GET_BIT(i, value);
				}
				break;
			case 0x186:
				for (int i = 0; i < 8; i++)
				{
					m_voices[i + 16].m_Noise = GET_BIT(i, value);
				}
				break;
			case 0x188:
				// TODO verify hi/lo order of all of these
				m_VMIXL.lo = value;
				break;
			case 0x18A:
				m_VMIXL.hi = value;
				break;
			case 0x18C:
				m_VMIXEL.lo = value;
				break;
			case 0x18E:
				m_VMIXEL.hi = value;
				break;
			case 0x190:
				m_VMIXR.lo = value;
				break;
			case 0x192:
				m_VMIXR.hi = value;
				break;
			case 0x194:
				m_VMIXER.lo = value;
				break;
			case 0x196:
				m_VMIXER.hi = value;
				break;
			case 0x198:
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
				for (int i = 0; i < 16; i++)
				{
					m_voices[i].m_KeyOn = GET_BIT(i, value);
				}
				break;
			case 0x1A2:
				for (int i = 0; i < 8; i++)
				{
					m_voices[i + 16].m_KeyOn = GET_BIT(i, value);
				}
				break;
			case 0x1A4:
				for (int i = 0; i < 16; i++)
				{
					m_voices[i].m_KeyOff = GET_BIT(i, value);
				}
				break;
			case 0x1A6:
				for (int i = 0; i < 8; i++)
				{
					m_voices[i + 16].m_KeyOff = GET_BIT(i, value);
				}
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
				for (int i = 0; i < 16; i++)
				{
					m_voices[i].m_ENDX = GET_BIT(i, value);
				}
				break;
			case 0x342:
				for (int i = 0; i < 8; i++)
				{
					m_voices[i + 16].m_ENDX = GET_BIT(i, value);
				}
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
				break;
			case 0x76A:
				m_AVOL.right = static_cast<s16>(value);
				break;
			case 0x76C:
				m_BVOL.left = static_cast<s16>(value);
				break;
			case 0x76E:
				m_BVOL.right = static_cast<s16>(value);
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
			default:
				Console.WriteLn("UNHANDLED SPU[%d] WRITE %04x -> %04x", m_Id, value, addr);
				pxAssertMsg(false, "Unhandled SPU Write");
		}
	}

	void SPUCore::Reset()
	{
		Console.WriteLn("SPU[%d] Reset", m_Id);
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
