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

		void GenSample();

		[[nodiscard]] u16 Read(u32 addr) const;
		void Write(u32 addr, u16 value);

		// The new (for SPU2) full addr regs are a separate range
		[[nodiscard]] u16 ReadAddr(u32 addr) const;
		void WriteAddr(u32 addr, u16 value);

		void Reset()
		{
			m_PitchMod = false;
			m_KeyOn = false;
			m_KeyOff = false;
			m_ENDX = false;

			m_DecodeBuf.Reset();
			m_DecodeHist1 = 0;
			m_DecodeHist2 = 0;
			m_Counter = 0;
			m_Pitch = 0;
			m_SSA.full = 0;
			m_NAX.full = 0;
			m_LSA.full = 0;
			m_CustomLoop = false;
			m_CurHeader.bits = 0;
			m_ADSR.Reset();
			m_Volume.Reset();
		}

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
		void UpdateBlockHeader();

		SPUCore& m_SPU;
		s32 m_Id{0};

		SampleFifo<s16, 0x20> m_DecodeBuf{};
		s16 m_DecodeHist1{0};
		s16 m_DecodeHist2{0};
		u32 m_Counter{0};

		u16 m_Pitch{0};
		s16 m_Out{0};

		Reg32 m_SSA{0};
		Reg32 m_NAX{0};
		Reg32 m_LSA{0};
		bool m_CustomLoop{false};

		ADPCMHeader m_CurHeader{};

		ADSR m_ADSR{};
		VolumePair m_Volume{};
	};
} // namespace SPU
