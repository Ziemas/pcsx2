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

		SPUCore& m_SPU;
		u32 m_Id{0};

		FIFO<u16, 0x20> DecodeBuf{};

		bool m_Noise{false};
		bool m_PitchMod{false};
		bool m_KeyOn{false};
		bool m_KeyOff{false};
	};
} // namespace SPU
