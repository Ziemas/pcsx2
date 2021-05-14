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

namespace SPU
{
	s16 SPUCore::GenSample()
	{
		for (auto& v : m_voices)
		{
			v.GenSample();
		}

		return 0;
	}

	u16 SPUCore::Read(u32 addr)
	{

		switch (addr)
		{
			//case 0x19A: // core att, probably not readable
			//	break;
			case 0x1b0:
				return m_adma;
			default:
				Console.WriteLn("UNHANDLED SPU[%d] READ ---- <- %04x", m_id, addr);
				pxAssertMsg(false, "Unhandled SPU Write");
				return 0;
		}

		return 0;
	}

	void SPUCore::Write(u32 addr, u16 value)
	{
		switch (addr)
		{
			case 0x19A:
				m_attr.bits = value;
				break;
			case 0x1B0:
				m_adma = value;
				break;
			default:
				Console.WriteLn("UNHANDLED SPU[%d] WRITE %04x -> %04x", m_id, value, addr);
				pxAssertMsg(false, "Unhandled SPU Read");
		}

		return;
	}
} // namespace SPU
