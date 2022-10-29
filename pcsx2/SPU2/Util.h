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
#include "GS/GSVector.h"
#include "Envelope.h"
#include <algorithm>

namespace SPU
{
	static __fi void SET_HIGH(u32& number, u32 upper) { number = (number & 0x0000FFFF) | upper << 16; }
	static __fi void SET_LOW(u32& number, u32 low) { number = (number & 0xFFFF0000) | low; }
	static __fi u32 GET_HIGH(u32 number) { return (number & 0xFFFF0000) >> 16; }
	static __fi u32 GET_LOW(u32 number) { return number & 0x0000FFFF; }
	static __fi u32 GET_BIT(u32 idx, u32 value) { return (value >> idx) & 1; }
	static __fi void SET_BIT(u32& number, u32 idx) { number |= (1 << idx); }

	static __fi s16 ApplyVolume(s16 sample, s32 volume)
	{
		return (sample * volume) >> 15;
	}

	static __fi s16 hsum(GSVector8i& vec1, GSVector8i& vec2)
	{

		GSVector8i ymm0 = vec1.adds16(vec2);

		GSVector4i xmm0 = ymm0.extract<0>();
		GSVector4i xmm1 = ymm0.extract<1>();
		xmm0 = xmm0.adds16(xmm1);

		xmm1 = xmm0.wzyx();
		xmm0 = xmm0.adds16(xmm1);

		xmm1 = xmm0.yxxx();
		xmm0 = xmm0.adds16(xmm1);

		xmm1 = xmm0.yxxxl();
		xmm0 = xmm0.adds16(xmm1);

		return xmm0.I16[0];
	}

	struct PlainVolReg
	{
		s16 left;
		s16 right;
	};

	struct AudioSample
	{
		AudioSample() = default;
		AudioSample(s16 left, s16 right)
			: left(left)
			, right(right)
		{
		}

		s16 left{0};
		s16 right{0};

		__fi void Mix(AudioSample src, bool lgate, bool rgate)
		{
			if (lgate)
				left = static_cast<s16>(std::clamp<s32>(left + src.left, INT16_MIN, INT16_MAX));
			if (rgate)
				right = static_cast<s16>(std::clamp<s32>(right + src.right, INT16_MIN, INT16_MAX));
		}

		__fi void Mix(AudioSample src)
		{
			left = static_cast<s16>(std::clamp<s32>(left + src.left, INT16_MIN, INT16_MAX));
			right = static_cast<s16>(std::clamp<s32>(right + src.right, INT16_MIN, INT16_MAX));
		}

		__fi void Volume(PlainVolReg vol)
		{
			left = ApplyVolume(left, vol.left);
			right = ApplyVolume(right, vol.right);
		}

		__fi void Volume(VolumePair vol)
		{
			left = ApplyVolume(left, vol.left.GetCurrent());
			right = ApplyVolume(right, vol.right.GetCurrent());
		}
	};

	union Reg32
	{
		u32 full;

		BitField<u32, u16, 16, 16> hi;
		BitField<u32, u16, 0, 16> lo;
	};
} // namespace SPU
