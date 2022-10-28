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

#include "Noise.h"
#include <array>

static constexpr std::array<s8, 64> noise_add = {
	1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1};

static constexpr std::array<u8, 5> noise_freq_add = {
	0, 84, 140, 180, 210};


void Noise::Run()
{
	u32 level = 0x8000 >> (m_Clock >> 2);
	level <<= 16;
	m_Count += 0x10000;
	m_Count += noise_freq_add[m_Clock & 3];

	if ((m_Count & 0xFFFF) >= noise_freq_add[m_Clock & 3])
	{
		m_Count += 0x10000;
		m_Count -= noise_freq_add[m_Clock & 3];
	}

	if (m_Count >= level)
	{
		while (m_Count >= level)
			m_Count -= level;

		m_Value = (m_Value << 1) | noise_add[(m_Value >> 10) & 63];
	}
}
