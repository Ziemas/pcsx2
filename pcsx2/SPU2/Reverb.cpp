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
#include "SpuCore.h"

namespace SPU
{
	static constexpr u32 NUM_TAPS = 39;
	alignas(32) static const union
	{
		std::array<GSVector8i, 3> vec;
		// clang-format off
		std::array<s16, NUM_TAPS> array = {
			-1,		0,		2,		0,		-10,
			0,		35,		0,		-103,	0,
			266,	0,		-616,	0,		1332,
			0,		-2960,	0,		10246,	16384,
			10246,	0,		-2960,	0,		1332,
			0,		-616,	0,		266,	0,
			-103,	0,		35,		0,		-10,
			0,		2,		0,		-1,		};
		// clang-format on
	} FilterCoefficients{};

	inline s16 Reverb::DownSample(AudioSample in)
	{
		m_ReverbIn[0].Push(in.left);
		m_ReverbIn[1].Push(in.right);

		const s16* const samples = m_ReverbIn[m_Phase].Get();
		std::array<GSVector8i, 3> vec{
			GSVector8i::load<false>(&samples[0]),
			GSVector8i::load<false>(&samples[16]),
			GSVector8i::load<false>(&samples[32])};

		vec[0] = vec[0].mul16hrs(FilterCoefficients.vec[0]);
		vec[1] = vec[1].mul16hrs(FilterCoefficients.vec[1]);
		vec[2] = vec[2].mul16hrs(FilterCoefficients.vec[2]);
		vec[0] = vec[0].adds16(vec[1].adds16(vec[2]));

		return hsum(vec[0]);
	}

	inline AudioSample Reverb::UpSample(s16 in)
	{
		m_ReverbOut[0].Push(m_Phase ? in : 0);
		m_ReverbOut[1].Push(m_Phase ? 0 : in);

		const s16* const left_samples = m_ReverbOut[0].Get();
		const s16* const right_samples = m_ReverbOut[1].Get();
		std::array<GSVector8i, 3> lvec{
			GSVector8i::load<false>(&left_samples[0]),
			GSVector8i::load<false>(&left_samples[16]),
			GSVector8i::load<false>(&left_samples[32])};
		std::array<GSVector8i, 3> rvec{
			GSVector8i::load<false>(&right_samples[0]),
			GSVector8i::load<false>(&right_samples[16]),
			GSVector8i::load<false>(&right_samples[32])};

		lvec[0] = lvec[0].mul16hrs(FilterCoefficients.vec[0]);
		lvec[1] = lvec[1].mul16hrs(FilterCoefficients.vec[1]);
		lvec[2] = lvec[2].mul16hrs(FilterCoefficients.vec[2]);

		rvec[0] = rvec[0].mul16hrs(FilterCoefficients.vec[0]);
		rvec[1] = rvec[1].mul16hrs(FilterCoefficients.vec[1]);
		rvec[2] = rvec[2].mul16hrs(FilterCoefficients.vec[2]);

		lvec[0] = lvec[0].adds16(lvec[1]);
		lvec[0] = lvec[0].adds16(lvec[2]);

		rvec[0] = rvec[0].adds16(rvec[1]);
		rvec[0] = rvec[0].adds16(rvec[2]);

		// Multiply to maintain same level of gain
		return {
			static_cast<s16>(std::clamp<s32>(hsum(lvec[0]) * 2, INT16_MIN, INT16_MAX)),
			static_cast<s16>(std::clamp<s32>(hsum(rvec[0]) * 2, INT16_MIN, INT16_MAX))};
	}

	inline s32 Reverb::IIASM(const s16 vIIR, const s16 sample)
	{
		if (vIIR == INT16_MIN)
		{
			if (sample == INT16_MIN)
				return 0;

			return sample * -0x10000;
		}

		return sample * (INT16_MAX - vIIR);
	}

	inline s16 Reverb::Sat(s32 sample)
	{
		return static_cast<s16>(std::clamp<s32>(sample, INT16_MIN, INT16_MAX));
	}

	inline s16 Reverb::Neg(s16 sample)
	{
		if (sample == INT16_MIN)
			return INT16_MAX;

		return static_cast<s16>(-sample);
	}

	inline u32 Reverb::Offset(s32 offset) const
	{
		u32 address = m_pos + offset;

		// this off by one from real behaviour in the ESA > EEA case
		// where pos is stuck in place

		if (address > m_EEA.full)
			address -= m_EEA.full - m_ESA.full;

		return address;
	}

	inline s16 Reverb::Read(u32 address, s32 offset)
	{
		m_SPU.TestIrq(Offset(address + offset));
		return static_cast<s16>(m_SPU.Ram(Offset(address + offset)));
	}

	inline void Reverb::Write(u32 address, s16 sample)
	{
		if (m_Enable)
		{
			m_SPU.TestIrq(Offset(address));
			m_SPU.WriteMem(Offset(address), static_cast<u16>(sample));
		}
	}

	AudioSample Reverb::Run(AudioSample input)
	{
		// down-sample input
		auto in = DownSample(input);

		const s16 SAME_SIDE_IN = Sat((((Read(dSAME[m_Phase ^ 0].full) * vWALL) >> 14) + ((in * vIN[m_Phase]) >> 14)) >> 1);
		const s16 DIFF_SIDE_IN = Sat((((Read(dDIFF[m_Phase ^ 1].full) * vWALL) >> 14) + ((in * vIN[m_Phase]) >> 14)) >> 1);
		const s16 SAME_SIDE = Sat((((SAME_SIDE_IN * vIIR) >> 14) + (IIASM(vIIR, Read(mSAME[m_Phase].full, -1)) >> 14)) >> 1);
		const s16 DIFF_SIDE = Sat((((DIFF_SIDE_IN * vIIR) >> 14) + (IIASM(vIIR, Read(mDIFF[m_Phase].full, -1)) >> 14)) >> 1);

		Write(mSAME[m_Phase].full, SAME_SIDE);
		Write(mDIFF[m_Phase].full, DIFF_SIDE);

		const s32 COMB = ((Read(mCOMB1[m_Phase].full) * vCOMB1) >> 14) +
						 ((Read(mCOMB2[m_Phase].full) * vCOMB2) >> 14) +
						 ((Read(mCOMB3[m_Phase].full) * vCOMB3) >> 14) +
						 ((Read(mCOMB4[m_Phase].full) * vCOMB4) >> 14);

		const s16 APF1 = Read(mAPF1[m_Phase].full - dAPF[0].full);
		const s16 APF2 = Read(mAPF2[m_Phase].full - dAPF[1].full);
		const s16 APF1_OUT = Sat((COMB + ((APF1 * Neg(vAPF1)) >> 14)) >> 1);
		const s16 APF2_OUT = Sat(APF1 + ((((APF1_OUT * vAPF1) >> 14) + ((APF2 * Neg(vAPF2)) >> 14)) >> 1));
		const s16 OUT = Sat(APF2 + ((APF2_OUT * vAPF2) >> 15));

		Write(mAPF1[m_Phase].full, APF1_OUT);
		Write(mAPF2[m_Phase].full, APF2_OUT);

		// up-sample output
		auto output = UpSample(OUT);

		m_Phase ^= 1;
		if (m_Phase)
			m_pos++;

		// if esa > eea pos sticks in place
		if (m_pos > m_EEA.full)
		{
			m_pos = m_ESA.full;
		}

		return output;
	}
} // namespace SPU
