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

#include <array>
#include "common/Pcsx2Types.h"

#pragma once

namespace SPU
{
	class SPUCore;

	class Voice
	{
	public:
		Voice(SPUCore& spu, u32 id)
			: m_spu(spu)
			, m_id(id)
		{
		}

		s16 run();

	private:
		SPUCore& m_spu;
		u32 m_id;
	};

	class SPUCore
	{
	public:
		SPUCore(u32 id)
			: m_id(id)
		{
		}

		void write16();
		u16 read16(u32 addr);

	private:
		u32 m_id;
		std::array<u16, 1024 * 1024 * 2> m_RAM;
		// clang-format off
	Voice m_voices[24] = {
		{*this, 0},  {*this, 1},
		{*this, 2},  {*this, 3},
		{*this, 4},  {*this, 5},
		{*this, 6},  {*this, 7},
		{*this, 8},  {*this, 9},
		{*this, 10}, {*this, 11},
		{*this, 12}, {*this, 13},
		{*this, 14}, {*this, 15},
		{*this, 16}, {*this, 17},
		{*this, 18}, {*this, 19},
		{*this, 20}, {*this, 21},
		{*this, 22}, {*this, 23},
	};
		// clang-format on
	};
} // namespace SPU
