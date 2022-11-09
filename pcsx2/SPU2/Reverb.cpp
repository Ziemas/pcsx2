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

	inline static s32 IIASM(const s16 vIIR, const s16 sample)
	{
		if (vIIR == INT16_MIN)
		{
			if (sample == INT16_MIN)
				return 0;

			return sample * -0x10000;
		}

		return sample * (INT16_MAX - vIIR);
	}

	inline static s16 ReverbSat(s32 sample)
	{
		return static_cast<s16>(std::clamp<s32>(sample, INT16_MIN, INT16_MAX));
	}

	inline static s16 ReverbNeg(s16 sample)
	{
		if (sample == INT16_MIN)
			return INT16_MAX;

		return static_cast<s16>(-sample);
	}

	inline u32 Reverb::Offset(s32 offset) const
	{
		uint32_t address = m_pos + offset;
		uint32_t size = m_EEA.full - m_ESA.full;

		if (size == 0)
			return 0;

		address = m_ESA.full + (address % size);

		return address;
	}

	inline s16 Reverb::RD_RVB(s32 address, s32 offset)
	{
		m_SPU.TestIrq(Offset(address + offset));
		return static_cast<s16>(m_SPU.Ram(Offset(address + offset)));
	}

	inline void Reverb::WR_RVB(s32 address, s16 sample)
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

		const s16 SAME_SIDE_IN = ReverbSat((((RD_RVB(static_cast<s32>(dSAME[m_Phase ^ 0].full)) * vWALL) >> 14) + ((in * vIN[m_Phase]) >> 14)) >> 1);
		const s16 DIFF_SIDE_IN = ReverbSat((((RD_RVB(static_cast<s32>(dDIFF[m_Phase ^ 1].full)) * vWALL) >> 14) + ((in * vIN[m_Phase]) >> 14)) >> 1);
		const s16 SAME_SIDE = ReverbSat((((SAME_SIDE_IN * vIIR) >> 14) + (IIASM(vIIR, RD_RVB(static_cast<s32>(mSAME[m_Phase].full), -1)) >> 14)) >> 1);
		const s16 DIFF_SIDE = ReverbSat((((DIFF_SIDE_IN * vIIR) >> 14) + (IIASM(vIIR, RD_RVB(static_cast<s32>(mDIFF[m_Phase].full), -1)) >> 14)) >> 1);

		WR_RVB(static_cast<s32>(mSAME[m_Phase].full), SAME_SIDE);
		WR_RVB(static_cast<s32>(mDIFF[m_Phase].full), DIFF_SIDE);

		const s32 COMB = ((RD_RVB(static_cast<s32>(mCOMB1[m_Phase].full)) * vCOMB1) >> 14) +
						 ((RD_RVB(static_cast<s32>(mCOMB2[m_Phase].full)) * vCOMB2) >> 14) +
						 ((RD_RVB(static_cast<s32>(mCOMB3[m_Phase].full)) * vCOMB3) >> 14) +
						 ((RD_RVB(static_cast<s32>(mCOMB4[m_Phase].full)) * vCOMB4) >> 14);

		const s16 APF1 = RD_RVB(static_cast<s32>(mAPF1[m_Phase].full - dAPF[0].full));
		const s16 APF2 = RD_RVB(static_cast<s32>(mAPF2[m_Phase].full - dAPF[1].full));
		const s16 MDA = ReverbSat((COMB + ((APF1 * ReverbNeg(vAPF1)) >> 14)) >> 1);
		const s16 MDB = ReverbSat(APF1 + ((((MDA * vAPF1) >> 14) + ((APF2 * ReverbNeg(vAPF2)) >> 14)) >> 1));
		const s16 IVB = ReverbSat(APF2 + ((MDB * vAPF2) >> 15));

		WR_RVB(static_cast<s32>(mAPF1[m_Phase].full), MDA);
		WR_RVB(static_cast<s32>(mAPF2[m_Phase].full), MDB);

		// up-sample output
		auto output = UpSample(IVB);

		m_Phase ^= 1;
		if (m_Phase)
			m_pos++;
		if (m_pos >= m_EEA.full - m_ESA.full + 1)
			m_pos = 0;

		return output;
	}
} // namespace SPU
