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

class Noise
{
public:
	void Run();
	void SetClock(u8 clock) { m_Clock = clock; }
	[[nodiscard]] s16 Get() const { return static_cast<s16>(m_Value); }
	void Reset()
	{
		m_Value = 0;
		m_Count = 0;
		m_Clock = 0;
	}

private:
	u32 m_Value{0};
	u32 m_Count{0};
	u8 m_Clock{0};
};
