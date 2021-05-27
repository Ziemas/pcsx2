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

	void SPUCore::WriteMem(u32& addr, u16 value)
	{
		printf("SPU[%d] Writing %04x to %08x", addr, value);
		m_RAM[addr] = value;
		++addr &= 0xFFFFF;
	}

	u16 SPUCore::Read(u32 addr)
	{
		if (addr < 0x180)
		{
			// voice
		}
		switch (addr)
		{
			case 0x19A:
				return m_Attr.bits;
			case 0x1b0:
				return m_Adma.bits;
			case 0x33c:
				return m_Reverb.m_EEA.hi.GetValue();
			case 0x344:
				return m_Stat.bits;
			default:
				Console.WriteLn("UNHANDLED SPU[%d] READ ---- <- %04x", m_Id, addr);
				pxAssertMsg(false, "Unhandled SPU Write");
				return 0;
		}

		return 0;
	}

	void SPUCore::Write(u32 addr, u16 value)
	{
		if (addr < 0x180)
		{
		}
		switch (addr)
		{
			case 0x180:
				for (int i = 0; i < 16; i++)
				{
					m_voices[i].m_PitchMod = (value >> i) & 1;
				}
				break;
			case 0x182:
				for (int i = 16; i < 8; i++)
				{
					m_voices[i].m_PitchMod = (value >> i) & 1;
				}
				break;
			case 0x184:
				for (int i = 0; i < 16; i++)
				{
					m_voices[i].m_Noise = (value >> i) & 1;
				}
				break;
			case 0x186:
				for (int i = 16; i < 8; i++)
				{
					m_voices[i].m_Noise = (value >> i) & 1;
				}
				break;
			case 0x19A:
				m_Attr.bits = value;
				break;
			case 0x1B0:
				m_Adma.bits = value;
				break;
			case 0x1A0:
				for (int i = 0; i < 16; i++)
				{
					m_voices[i].m_KeyOn = (value >> i) & 1;
				}
				break;
			case 0x1A2:
				for (int i = 16; i < 8; i++)
				{
					m_voices[i].m_KeyOn = (value >> i) & 1;
				}
				break;
			case 0x1A4:
				for (int i = 0; i < 16; i++)
				{
					m_voices[i].m_KeyOff = (value >> i) & 1;
				}
				break;
			case 0x1A6:
				for (int i = 16; i < 8; i++)
				{
					m_voices[i].m_KeyOff = (value >> i) & 1;
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
				WriteMem(m_InternalTSA, value);
				break;
			case 0x2E0:
				m_Reverb.m_pos = 0;
				m_Reverb.m_EEA.hi = value & 0x3f;
				break;
			case 0x2E2:
				m_Reverb.m_pos = 0;
				m_Reverb.m_EEA.lo = value;
				break;
			//case 0x344:
			//	// SPU Status R/O
			//	break;
			default:
				Console.WriteLn("UNHANDLED SPU[%d] WRITE %04x -> %04x", m_Id, value, addr);
				pxAssertMsg(false, "Unhandled SPU Read");
		}

		return;
	}
} // namespace SPU
