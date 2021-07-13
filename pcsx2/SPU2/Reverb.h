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
	class Reverb
	{
	public:
		AudioSample Run(AudioSample input);

		Reg32 m_ESA{0};
		Reg32 m_EEA{0};
		u32 m_pos{0};

	private:
		static constexpr u32 NUM_TAPS = 39;

		template <size_t len>
		struct SampleBuffer
		{
			u32 m_Pos{0};
			std::array<AudioSample, len> m_Buffer{};

			void Push(AudioSample sample)
			{
				m_Pos = (m_Pos + 1) % len;
				m_Buffer[m_Pos] = sample;
			}

			[[nodiscard]] const AudioSample& Get(u32 index) const
			{
				return m_Buffer[(m_Pos + index + 1) % len];
			}
		};
		SampleBuffer<NUM_TAPS> m_ReverbIn{};
		SampleBuffer<NUM_TAPS> m_ReverbOut{};

		s16 DownSample(AudioSample in);
		AudioSample UpSample(s16 in);

		u32 m_SamplePos{0};
		u32 m_Phase{0};
	};
} // namespace SPU
