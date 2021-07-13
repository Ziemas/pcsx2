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

#include "Reverb.h"

namespace SPU
{
	static constexpr u32 NUM_TAPS = 39;
	static constexpr std::array<s32, NUM_TAPS> FilterCoefficients = {
		-1,
		0,
		2,
		0,
		-10,
		0,
		35,
		0,
		-103,
		0,
		266,
		0,
		-616,
		0,
		1332,
		0,
		-2960,
		0,
		10246,
		16384,
		10246,
		0,
		-2960,
		0,
		1332,
		0,
		-616,
		0,
		266,
		0,
		-103,
		0,
		35,
		0,
		-10,
		0,
		2,
		0,
		-1,
	};

	s16 Reverb::DownSample(AudioSample in)
	{
		m_ReverbIn.Push(in);

		s32 down{0};
		for (u32 i = 0; i < NUM_TAPS; i++)
		{
			auto s = m_ReverbIn.Get(i);
			if (m_Phase)
				down += s.right * FilterCoefficients[i];
			else
				down += s.left * FilterCoefficients[i];
		}

		down >>= 15;
		return static_cast<s16>(std::clamp<s32>(down, INT16_MIN, INT16_MAX));
	}

	AudioSample Reverb::UpSample(s16 in)
	{
		AudioSample up(0, 0);

		if (m_Phase)
			up.right = in;
		else
			up.left = in;

		m_ReverbOut.Push(up);

		s32 left{0}, right{0};
		for (u32 i = 0; i < NUM_TAPS; i++)
		{
			left += m_ReverbOut.Get(i).left * FilterCoefficients[i];
			right += m_ReverbOut.Get(i).right * FilterCoefficients[i];
		}

		left >>= 14;
		right >>= 14;

		AudioSample out(static_cast<s16>(std::clamp<s32>(left, INT16_MIN, INT16_MAX)),
			static_cast<s16>(std::clamp<s32>(right, INT16_MIN, INT16_MAX)));

		return out;
	}

	AudioSample Reverb::Run(AudioSample input)
	{
		// down-sample input
		auto in = DownSample(input);
        // up-sample output
        auto output = UpSample(in);

		m_Phase ^= 1;
		m_SamplePos++;
		return output;
	}
} // namespace SPU