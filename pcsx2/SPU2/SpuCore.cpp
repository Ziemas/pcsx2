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
	s16 SPUCore::GenSample()
	{
		s16 sample = 0;
		for (auto& v : m_voices)
		{
			s16 nexts = v.GenSample();
			sample = std::clamp(sample+nexts, -0x8000, 0x7fff);
		}

		return sample;
	}

	void SPUCore::WriteMem(u32& addr, u16 value)
	{
		Console.WriteLn("SPU[%d] Writing %04x to %08x", m_Id, addr, value);
		m_RAM[addr] = value;
		++addr &= 0xFFFFF;
	}

	void SPUCore::DmaWrite(u16* madr, u32 size)
	{
		memcpy(&m_RAM[m_TSA.full], madr, size * 2);
		m_Stat.DMABusy = false;
		m_Stat.DMAReady = true;
		if (m_Id == 0)
		{
			HW_DMA4_MADR += size * 2;
			spu2DMA4Irq();
		}
		else
		{
			HW_DMA7_MADR += size * 2;
			spu2DMA7Irq();
		}
	}

	void SPUCore::DmaRead(u16* madr, u32 size)
	{
		memcpy(madr, &m_RAM[m_TSA.full], size * 2);
		if (m_Id == 0)
		{
			HW_DMA4_MADR += size * 2;
			spu2DMA4Irq();
		}
		else
		{
			HW_DMA7_MADR += size * 2;
			spu2DMA7Irq();
		}
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
			case 0x188:
				return m_VMIXL.lo.GetValue();
			case 0x18A:
				return m_VMIXL.hi.GetValue();
			case 0x190:
				return m_VMIXR.lo.GetValue();
			case 0x192:
				return m_VMIXR.hi.GetValue();
			case 0x194:
				return m_VMIXER.hi.GetValue();
			case 0x196:
				return m_VMIXER.hi.GetValue();
			case 0x198:
				return m_MMIX;
			case 0x19A:
				return m_Attr.bits;
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
			default:
				Console.WriteLn("UNHANDLED SPU[%d] READ ---- <- %04x", m_Id, addr);
				pxAssertMsg(false, "Unhandled SPU Read");
				return 0;
		}

		return 0;
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
				m_MMIX = value;
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
				break;
			case 0x2E0:
				m_Reverb.m_pos = 0;
				m_Reverb.m_ESA.hi = value & 0x3f;
				break;
			case 0x2E2:
				m_Reverb.m_pos = 0;
				m_Reverb.m_ESA.lo = value;
				break;
			case 0x33C:
				m_Reverb.m_EEA.hi = value;
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
			default:
				Console.WriteLn("UNHANDLED SPU[%d] WRITE %04x -> %04x", m_Id, value, addr);
				pxAssertMsg(false, "Unhandled SPU Write");
		}

		return;
	}
} // namespace SPU
