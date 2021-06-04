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

#pragma once

#include "common/Pcsx2Types.h"
#include "common/fifo.h"
#include "Util.h"

namespace SPU
{
	class SPUCore;

	struct Voice
	{
		Voice(SPUCore& spu, u32 id)
			: m_SPU(spu)
			, m_Id(id)
		{
		}


		s16 GenSample();

		u16 Read(u32 addr);
		void Write(u32 addr, u16 value);

		// The new (for SPU2) full addr regs are a separate range
		u16 ReadAddr(u32 addr);
		void WriteAddr(u32 addr, u16 value);

		SPUCore& m_SPU;
		u32 m_Id{0};

		FIFO<u16, 0x20> DecodeBuf{};

		u32 m_Pitch{0};

		Reg32 m_SSA{0};
		Reg32 m_NAX{0};

		// TODO Envelope
		u32 m_ADSR1{0};
		u32 m_ADSR2{0};
		u32 m_ENVX{0};

		// TODO vol envelope
		u32 m_Voll{0};
		u32 m_Volr{0};

		bool m_Noise{false};
		bool m_PitchMod{false};
		bool m_KeyOn{false};
		bool m_KeyOff{false};
		bool m_ENDX{false};
	};
} // namespace SPU
