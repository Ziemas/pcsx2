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
	class SPUCore;
	static constexpr std::array<std::array<s16, 4>, 256> gaussianConstructTable()
	{
		std::array<std::array<s16, 4>, 256> result = {};
		double table[512] = {};
		for (u32 n = 0; n < 512; n++)
		{
			double k = 0.5 + n;
			double s = (gcem::sin(GCEM_PI * k * 2.048 / 1024));
			double t = (gcem::cos(GCEM_PI * k * 2.000 / 1023) - 1) * 0.50;
			double u = (gcem::cos(GCEM_PI * k * 4.000 / 1023) - 1) * 0.08;
			double r = s * (t + u + 1.0) / k;
			table[511 - n] = r;
		}
		double sum = 0.0;
		for (u32 n = 0; n < 512; n++)
			sum += table[n];
		double scale = 0x7f80 * 128 / sum;
		for (u32 n = 0; n < 512; n++)
			table[n] *= scale;
		for (u32 phase = 0; phase < 256; phase++)
		{
			double sum = 0.0;
			sum += table[phase + 0];
			sum += table[phase + 256];
			sum += table[511 - phase];
			sum += table[255 - phase];
			double diff = (sum - 0x7f80) / 4;
			result[255 - phase][0] = gcem::round(table[phase + 0] - diff);
			result[255 - phase][1] = gcem::round(table[phase + 256] - diff);
			result[255 - phase][2] = gcem::round(table[511 - phase] - diff);
			result[255 - phase][3] = gcem::round(table[255 - phase] - diff);
		}

		return result;
	}

	static constexpr std::array<std::array<s16, 4>, 256> gaussianTable = gaussianConstructTable();

	s16 Voice::GenSample()
	{
		return 0;
	}

	u16 Voice::Read(u32 addr)
	{
		switch (addr)
		{
			case 0:
				return m_Voll;
			case 2:
				return m_Volr;
			case 4:
				return m_Pitch;
			case 6:
				return m_ADSR1;
			case 8:
				return m_ADSR2;
			case 10:
				return m_ENVX;
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
				m_Voll = value;
				return;
			case 2:
				m_Volr = value;
				return;
			case 4:
				m_Pitch = value;
				return;
			case 6:
				m_ADSR1 = value;
				return;
			case 8:
				m_ADSR2 = value;
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
			default:
				Console.WriteLn("UNHANDLED SPU[%d]:VOICE[%d] WriteAddr %04x -> %04x", m_SPU.m_Id, m_Id, value, addr);
				pxAssertMsg(false, "Unhandled SPU Write");
		}
	}



} // namespace SPU
