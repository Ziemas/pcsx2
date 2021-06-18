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

#include "Envelope.h"
#include <array>

namespace SPU
{
	//static constexpr std::array<u32, 0x7F> RateTableGenerate()
	//{
	//    std::array<u32, 0x7F> rates{};
	//    return rates;
	//}

	//static constexpr std::array<u32, 0x7F> RateTable = RateTableGenerate();

	void ADSR::Run() {}

	void Volume::Run() {}

	void Volume::Set(u16 volume)
	{
		VolReg reg{0};
		reg.bits = volume;

		if (!reg.EnableSweep)
		{
			m_Vol = static_cast<s16>(volume << 1);
			return;
		}

		m_Sweep.bits = reg.bits;
		// TODO Sweep
	}

	s16 Volume::Get()
	{
		return m_Vol;
	}
} // namespace SPU
