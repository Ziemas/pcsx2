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

#include "common/Bitfield.h"
#include "common/Pcsx2Types.h"

namespace SPU
{
	__fi static void SET_HIGH(u32& number, u32 upper) { number = (number & 0x0000FFFF) | upper << 16; }
	__fi static void SET_LOW(u32& number, u32 low) { number = (number & 0xFFFF0000) | low; }
	__fi static u32 GET_HIGH(u32 number) { return (number & 0xFFFF0000) >> 16; }
	__fi static u32 GET_LOW(u32 number) { return number & 0x0000FFFF; }
	__fi static u32 GET_BIT(u32 idx, u32 value) { return (value >> idx) & 1; }
	__fi static void SET_BIT(u32& number, u32 idx) { number |= (1 << idx); }

	union Reg32
	{
		u32 full;

		BitField<u32, u16, 16, 16> hi;
		BitField<u32, u16, 0, 16> lo;
	};
} // namespace SPU
