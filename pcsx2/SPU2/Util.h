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
#include <algorithm>

namespace SPU
{
	__fi static void SET_HIGH(u32& number, u32 upper) { number = (number & 0x0000FFFF) | upper << 16; }
	__fi static void SET_LOW(u32& number, u32 low) { number = (number & 0xFFFF0000) | low; }
	__fi static u32 GET_HIGH(u32 number) { return (number & 0xFFFF0000) >> 16; }
	__fi static u32 GET_LOW(u32 number) { return number & 0x0000FFFF; }
	__fi static u32 GET_BIT(u32 idx, u32 value) { return (value >> idx) & 1; }
	__fi static void SET_BIT(u32& number, u32 idx) { number |= (1 << idx); }

	__fi static s16 ApplyVolume(s16 sample, s32 volume)
	{
		return (sample * volume) >> 15;
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

		void Mix(AudioSample src, bool lgate, bool rgate)
		{
			if (lgate)
				left = static_cast<s16>(std::clamp<s32>(left + src.left, INT16_MIN, INT16_MAX));
			if (rgate)
				right = static_cast<s16>(std::clamp<s32>(right + src.right, INT16_MIN, INT16_MAX));
		}

		void Volume(PlainVolReg vol)
		{
			left = ApplyVolume(left, vol.left);
			right = ApplyVolume(right, vol.right);
		}
	};

	union Reg32
	{
		u32 full;

		BitField<u32, u16, 16, 16> hi;
		BitField<u32, u16, 0, 16> lo;
	};

	enum class TransferMode : u8
	{
		Stopped = 0,
		ManualWrite = 1,
		DMAWrite = 2,
		DMARead = 3,
	};

	union AttrReg
	{
		u16 bits;

		// iirc enable works like a reset switch here
		// driver flips enable on and expects DMA stuff to be reset
		BitField<u16, bool, 15, 1> Enable;
		BitField<u16, bool, 14, 1> OutputEnable;
		BitField<u16, u8, 8, 6> NoiseClock;
		BitField<u16, bool, 7, 1> EffectEnable;
		BitField<u16, bool, 6, 1> IRQEnable;
		BitField<u16, TransferMode, 4, 2> CurrentTransMode;
		// unknown if these do anything in ps2 mode
		BitField<u16, bool, 3, 1> ExtReverb;
		BitField<u16, bool, 2, 1> CDAReverb;
		BitField<u16, bool, 1, 1> EXTEnable;
		BitField<u16, bool, 0, 1> CDAEnable;
	};

	struct SpuSharedState
	{
		std::array<Reg32, 2> IRQA{0x800, 0x800};
		std::array<AttrReg, 2> Attr{};
		union IrqStat
		{
			u32 bits;

			BitField<u32, bool, 4, 1> BufferHalf;
			BitField<u32, bool, 3, 1> CauseC1;
			BitField<u32, bool, 2, 1> CauseC0;
		} IrqStat{0};
	};


} // namespace SPU
