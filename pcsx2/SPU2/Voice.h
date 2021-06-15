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
#include "common/Bitfield.h"
#include "common/fifo.h"
#include "Envelope.h"
#include "Util.h"

namespace SPU
{
	class SPUCore;

	class Voice
	{
	public:
		Voice(SPUCore& spu, u32 id)
			: m_SPU(spu)
			, m_Id(id)
		{
		}

		std::pair<s16, s16> GenSample();

		u16 Read(u32 addr);
		void Write(u32 addr, u16 value);

		// The new (for SPU2) full addr regs are a separate range
		u16 ReadAddr(u32 addr);
		void WriteAddr(u32 addr, u16 value);

		bool m_Noise{false};
		bool m_PitchMod{false};
		bool m_KeyOn{false};
		bool m_KeyOff{false};
		bool m_ENDX{false};

	private:
		union ADPCMHeader
		{
			u16 bits;
			BitField<u16, bool, 10, 1> LoopStart;
			BitField<u16, bool, 9, 1> LoopRepeat;
			BitField<u16, bool, 8, 1> LoopEnd;
			BitField<u16, u8, 4, 3> Filter;
			BitField<u16, u8, 0, 4> Shift;
		};

		void DecodeSamples();

		SPUCore& m_SPU;
		u32 m_Id{0};

		FIFO<s16, 0x20> m_DecodeBuf{};
		s16 m_DecodeHist1{0};
		s16 m_DecodeHist2{0};
		u32 m_Counter{0};

		u32 m_Pitch{0};

		Reg32 m_SSA{0};
		Reg32 m_NAX{0};
		Reg32 m_LSA{0};
		bool m_CustomLoop{false};

		ADPCMHeader m_CurHeader{};

		// TODO Envelope
		u32 m_ADSR1{0};
		u32 m_ADSR2{0};
		u32 m_ENVX{0};

		VolumePair m_Volume{};
	};
} // namespace SPU
