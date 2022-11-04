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
#include "gcem.hpp"

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

	union Reg32
	{
		u32 full;

		BitField<u32, u16, 16, 16> hi;
		BitField<u32, u16, 0, 16> lo;
	};

	union AddrVec
	{
		std::array<Reg32, 24> arr;
		std::array<GSVector8i, 3> vec;
	};

	union VoiceVec
	{
		std::array<s16, 24> arr;
		std::array<u16, 24> uarr;

		// lets fit in 3 of these so we have room to write
		// outx with an offset
		std::array<GSVector8i, 3> vec;
	};

	template <typename Tp, size_t Nm>
	class SampleFifo
	{
		static constexpr size_t Sz = (1 << static_cast<size_t>(gcem::ceil(gcem::log2(Nm))));

	public:
		void PopN(size_t n) { m_Rpos += n; }
		void Push(Tp val)
		{
			m_Buffer[mask(m_Wpos) | 0x0] = val;
			m_Buffer[mask(m_Wpos) | Sz] = val;
			m_Wpos++;
		}
		void PushSkipN(size_t n)
		{
			m_Wpos + n;
		}
		size_t Size() { return m_Wpos - m_Rpos; }
		Tp* Get()
		{
			return &m_Buffer[mask(m_Rpos)];
		}
		void Reset()
		{
			m_Buffer.fill(Tp{});
			m_Rpos = 0;
			m_Wpos = 0;
		}

	private:
		alignas(32) std::array<Tp, Sz << 1> m_Buffer{};
		size_t mask(size_t val) { return val & (Sz - 1); }
		size_t m_Rpos{0};
		size_t m_Wpos{0};
	};

	template <typename Tp, size_t Nm>
	class SampleBuffer
	{
		static constexpr size_t Sz = (1 << static_cast<size_t>(gcem::ceil(gcem::log2(Nm))));

	public:
		void Push(Tp sample)
		{
			m_Pos = mask(m_Pos + 1);
			m_Buffer[mask(m_Pos + Nm) | 0x0] = sample;
			m_Buffer[mask(m_Pos + Nm) | Sz] = sample;
		}

		Tp* Get()
		{
			return &m_Buffer[mask(m_Pos)];
		}

		void Reset()
		{
			m_Buffer.fill(Tp{});
			m_Pos = 0;
		}

	private:
		alignas(32) std::array<Tp, Sz << 1> m_Buffer{};

		size_t mask(size_t val) { return val & (Sz - 1); }
		size_t m_Pos{0};
	};


	static __fi s16 hsum(GSVector8i vec1)
	{
		GSVector4i xmm0 = vec1.extract<0>();
		GSVector4i xmm1 = vec1.extract<1>();
		xmm0 = xmm0.adds16(xmm1);

		xmm1 = xmm0.wzyx();
		xmm0 = xmm0.adds16(xmm1);

		xmm1 = xmm0.yxxx();
		xmm0 = xmm0.adds16(xmm1);

		xmm1 = xmm0.yxxxl();
		xmm0 = xmm0.adds16(xmm1);

		return xmm0.I16[0];
	}

	static __fi s16 hsum(GSVector8i vec1, GSVector8i vec2)
	{

		GSVector8i ymm0 = vec1.adds16(vec2);

		return hsum(ymm0);
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

	static __fi void ExpandVoiceBitfield(u16 value, Reg32& reg, GSVector8i& vec, bool hi)
	{
		u16 prev = hi ? reg.hi.GetValue() : reg.lo.GetValue();
		if (value != prev)
		{
			s32 limit = hi ? 8 : 16;
			for (int i = 0; i < limit; i++)
			{
				vec.U16[i] = GET_BIT(i, value) != 0U ? 0xffff : 0;
			}

			hi ? reg.hi.SetValue(value) : reg.lo.SetValue(value);
		}
	}
} // namespace SPU
