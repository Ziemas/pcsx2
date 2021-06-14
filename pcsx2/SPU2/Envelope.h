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

namespace SPU
{
	union ADSRReg
	{
		u32 bits;

		BitField<u32, u16, 16, 16> hi;
		BitField<u32, u16, 0, 16> lo;

		BitField<u32, bool, 31, 1> SustainExp;
		BitField<u32, bool, 30, 1> SustainDecr;
		BitField<u32, u8, 29, 1> Unused;
		BitField<u32, u8, 24, 5> SustainShift;
		BitField<u32, u8, 22, 2> SustainStep;
		BitField<u32, bool, 21, 1> ReleaseExp;
		BitField<u32, u8, 16, 5> ReleaseShift;

		BitField<u32, bool, 15, 1> AttackExp;
		BitField<u32, u8, 10, 5> AttackShift;
		BitField<u32, u8, 8, 2> AttackStep;
		BitField<u32, u8, 4, 4> DecayShift;
		BitField<u32, u8, 0, 4> SustainLevel;
	};

	class Envelope
	{
	};

	class ADSR : Envelope
	{
	public:
		void Run();
	};

	class VolReg : Envelope
	{
	public:
		void Run();
		void Set(s16 volume);
		s16 Get();

	private:
		s16 m_Vol{0};
	};

} // namespace SPU
