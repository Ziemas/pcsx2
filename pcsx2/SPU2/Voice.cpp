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
#include "gcem.hpp"
#include "common/Console.h"
#include "Voice.h"

namespace SPU
{
	// Integer math version of ps-adpcm coefs
	static constexpr s32 adpcm_coefs_i[5][2] = {
		{0, 0},
		{60, 0},
		{115, -52},
		{98, -55},
		{122, -60},
	};

	static constexpr std::array<std::array<s16, 4>, 256> gaussianConstructTable()
	{
		std::array<std::array<s16, 4>, 256> result = {};
		double table[512] = {};
		for (u32 n = 0; n < 512; n++)
		{
			double k = 0.5 + n;
			auto s = static_cast<double>((gcem::sin(GCEM_PI * k * 2.048 / 1024)));
			auto t = static_cast<double>((gcem::cos(GCEM_PI * k * 2.000 / 1023) - 1) * 0.50);
			auto u = static_cast<double>((gcem::cos(GCEM_PI * k * 4.000 / 1023) - 1) * 0.08);
			double r = s * (t + u + 1.0) / k;
			table[511 - n] = r;
		}
		double sum = 0.0;
		for (double n : table)
			sum += n;
		double scale = 0x7f80 * 128 / sum;
		for (double & n : table)
			n *= scale;
		for (u32 phase = 0; phase < 256; phase++)
		{
			double phase_sum = 0.0;
			phase_sum += table[phase + 0];
			phase_sum += table[phase + 256];
			phase_sum += table[511 - phase];
			phase_sum += table[255 - phase];
			double diff = (phase_sum - 0x7f80) / 4;
			result[255 - phase][0] = static_cast<s16>(gcem::round(table[phase + 0] - diff));
			result[255 - phase][1] = static_cast<s16>(gcem::round(table[phase + 256] - diff));
			result[255 - phase][2] = static_cast<s16>(gcem::round(table[511 - phase] - diff));
			result[255 - phase][3] = static_cast<s16>(gcem::round(table[255 - phase] - diff));
		}

		return result;
	}

	static constexpr std::array<std::array<s16, 4>, 256> gaussianTable = gaussianConstructTable();

	void Voice::DecodeSamples()
	{
		// This doesn't exactly match the real behaviour,
		// it seems to initially decode a bigger chunk
		// and then decode more data after a bit has drained
		if (m_DecodeBuf.Size() >= 16)
		{
			// sufficient data buffered
			return;
		}

		// TODO irq testing

		u32 data = m_SPU.Ram(m_NAX.full);
		for (int i = 0; i < 4; i++)
		{
			s32 sample = (s16)((data & 0xF) << 12);
			sample >>= m_CurHeader.Shift.GetValue();

			// TODO do the right thing for invalid shift/filter values
			sample += (adpcm_coefs_i[m_CurHeader.Filter.GetValue()][0] * m_DecodeHist1) >> 6;
			sample += (adpcm_coefs_i[m_CurHeader.Filter.GetValue()][1] * m_DecodeHist2) >> 6;

			// We do get overflow here otherwise, should we?
			sample = std::clamp(sample, -0x8000, 0x7FFF);

			m_DecodeHist2 = m_DecodeHist1;
			m_DecodeHist1 = sample;

			m_DecodeBuf.Push(sample);
			data >>= 4;
		}

		m_NAX.full++;

		if ((m_NAX.full & 0x7) == 0)
		{
			if (m_CurHeader.LoopEnd)
			{
				m_NAX.full = m_LSA.full;
				m_ENDX = true;

				if (!m_CurHeader.LoopRepeat)
				{
					m_ADSR.Stop();
				}
			}

			m_CurHeader.bits = m_SPU.Ram(m_NAX.full & ~0x7);
			if (m_CurHeader.LoopStart && !m_CustomLoop)
				m_LSA.full = m_NAX.full & ~0x7;

			m_NAX.full++;
		}
	}

	AudioSample Voice::GenSample()
	{
		if (m_KeyOff)
		{
			m_KeyOff = false;
			m_ADSR.Release();
			Console.WriteLn("SPU[%d]:VOICE[%d] Key Off", m_SPU.m_Id, m_Id);
		}
		if (m_KeyOn)
		{
			m_KeyOn = false;
			m_NAX.full = m_SSA.full;

			// Need to handle this for the first block otherwise we miss it in Decode
			// TODO figure out how to make this nicer
			m_CurHeader.bits = m_SPU.Ram(m_NAX.full & ~0x7);
			if (m_CurHeader.LoopStart && !m_CustomLoop)
				m_LSA.full = m_NAX.full & ~0x7;

			m_NAX.full++;
			m_ENDX = false;
			m_ADSR.Attack();
			m_Counter = 0;
			m_DecodeHist1 = 0;
			m_DecodeHist2 = 0;
			m_DecodeBuf.Reset();
			m_CustomLoop = false;
			Console.WriteLn("SPU[%d]:VOICE[%d] Key On, SSA %08x", m_SPU.m_Id, m_Id, m_SSA);
		}

		DecodeSamples();

		// Interpolation:
		//   nocash describes interpolation as happening to the 4 most recent samples
		//   do we start with the first sample and 0 for the 3 previous?
		//   or do we decode 4 samples and interpolate using the first 4?
		//   mednafen does the latter, and i could see why that might make sense

		// TODO noise
		s16 sample = 0;
		u32 index = (m_Counter & 0x0FF0) >> 4;
		sample += (m_DecodeBuf.Peek(0) * gaussianTable[index][0]) >> 15;
		sample += (m_DecodeBuf.Peek(1) * gaussianTable[index][1]) >> 15;
		sample += (m_DecodeBuf.Peek(2) * gaussianTable[index][2]) >> 15;
		sample += (m_DecodeBuf.Peek(3) * gaussianTable[index][3]) >> 15;

		// Step = VxPitch                  ;range +0000h..+FFFFh (0...705.6 kHz)
		//  IF PMON.Bit(x)=1 AND (x>0)      ;pitch modulation enable
		//    Factor = VxOUTX(x-1)          ;range -8000h..+7FFFh (prev voice amplitude)
		//    Factor = Factor+8000h         ;range +0000h..+FFFFh (factor = 0.00 .. 1.99)
		//    Step=SignExpand16to32(Step)   ;hardware glitch on VxPitch>7FFFh, make sign
		//    Step = (Step * Factor) SAR 15 ;range 0..1FFFFh (glitchy if VxPitch>7FFFh)
		//    Step=Step AND 0000FFFFh       ;hardware glitch on VxPitch>7FFFh, kill sign
		//  IF Step>3FFFh then Step=4000h   ;range +0000h..+3FFFh (0.. 176.4kHz)
		//  Counter = Counter + Step

		u32 step = m_Pitch;
		// TODO pitch mod
		step = std::min(step, 0x3FFFu);
		m_Counter += step;

		while (m_Counter >= 0x1000u)
		{
			m_Counter -= 0x1000u;
			m_DecodeBuf.Pop();
		}

		m_ADSR.Run();

		sample = ApplyVolume(sample, m_ADSR.Level());
		s16 left = ApplyVolume(sample, m_Volume.left.Get());
		s16 right = ApplyVolume(sample, m_Volume.right.Get());

		return AudioSample(left, right);
	}

	u16 Voice::Read(u32 addr)
	{
		switch (addr)
		{
			case 0:
				return m_Volume.left.Get();
			case 2:
				return m_Volume.right.Get();
			case 4:
				return m_Pitch;
			case 6:
				return m_ADSR.m_Reg.lo.GetValue();
			case 8:
				return m_ADSR.m_Reg.hi.GetValue();
			case 10:
				return m_ADSR.Level();
			default:
				Console.WriteLn("UNHANDLED SPU[%d]:VOICE[%d] READ ---- <- %04x", m_SPU.m_Id, m_Id, addr);
				pxAssertMsg(false, "Unhandled SPU Write");
				return 0;
		}
	}

	u16 Voice::ReadAddr(u32 addr)
	{
		switch (addr)
		{
			case 0:
				return m_SSA.hi.GetValue();
			case 2:
				return m_SSA.lo.GetValue();
			case 8:
				return m_NAX.hi.GetValue();
			case 10:
				return m_NAX.lo.GetValue();
			default:
				Console.WriteLn("UNHANDLED SPU[%d]:VOICE[%d] ReadAddr ---- <- %04x", m_SPU.m_Id, m_Id, addr);
				pxAssertMsg(false, "Unhandled SPU Write");
				return 0;
		}
	}

	void Voice::Write(u32 addr, u16 value)
	{
		switch (addr)
		{
			case 0:
				m_Volume.left.Set(value);
				return;
			case 2:
				m_Volume.right.Set(value);
				return;
			case 4:
				m_Pitch = value;
				return;
			case 6:
				m_ADSR.m_Reg.lo = value;
				m_ADSR.UpdateSettings();
				return;
			case 8:
				m_ADSR.m_Reg.hi = value;
				m_ADSR.UpdateSettings();
				return;
			default:
				Console.WriteLn("UNHANDLED SPU[%d]:VOICE[%d] WRITE %04x -> %04x", m_SPU.m_Id, m_Id, value, addr);
				pxAssertMsg(false, "Unhandled SPU Write");
		}
	}

	void Voice::WriteAddr(u32 addr, u16 value)
	{
		switch (addr)
		{
			case 0:
				m_SSA.hi = value;
				return;
			case 2:
				m_SSA.lo = value;
				return;
			case 4:
				m_LSA.hi = value;
				m_CustomLoop = true;
				return;
			case 6:
				m_LSA.lo = value;
				m_CustomLoop = true;
				return;
			default:
				Console.WriteLn("UNHANDLED SPU[%d]:VOICE[%d] WriteAddr %04x -> %04x", m_SPU.m_Id, m_Id, value, addr);
				pxAssertMsg(false, "Unhandled SPU Write");
		}
	}



} // namespace SPU
