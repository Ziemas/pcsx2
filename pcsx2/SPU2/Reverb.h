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
	class SPUCore;

	class Reverb
	{
	public:
		explicit Reverb(SPUCore& core)
			: m_SPU(core){};

		AudioSample Run(AudioSample input);

		bool m_Enable{false};

		Reg32 m_ESA{0};
		Reg32 m_EEA{0};
		u32 m_pos{0};

		Reg32 dAPF[2]{{0}, {0}};
		Reg32 mSAME[2]{{0}, {0}};
		Reg32 mCOMB1[2]{{0}, {0}};
		Reg32 mCOMB2[2]{{0}, {0}};
		Reg32 dSAME[2]{{0}, {0}};
		Reg32 mDIFF[2]{{0}, {0}};
		Reg32 mCOMB3[2]{{0}, {0}};
		Reg32 mCOMB4[2]{{0}, {0}};
		Reg32 dDIFF[2]{{0}, {0}};
		Reg32 mAPF1[2]{{0}, {0}};
		Reg32 mAPF2[2]{{0}, {0}};

		s16 vIIR{0};
		s16 vCOMB1{0};
		s16 vCOMB2{0};
		s16 vCOMB3{0};
		s16 vCOMB4{0};
		s16 vWALL{0};
		s16 vAPF1{0};
		s16 vAPF2{0};
		s16 vIN[2]{0};

		void Reset()
		{
			for (auto& r : dAPF)
				r.full = 0;
			for (auto& r : mSAME)
				r.full = 0;
			for (auto& r : mCOMB1)
				r.full = 0;
			for (auto& r : mCOMB2)
				r.full = 0;
			for (auto& r : dSAME)
				r.full = 0;
			for (auto& r : mDIFF)
				r.full = 0;
			for (auto& r : mCOMB3)
				r.full = 0;
			for (auto& r : mCOMB4)
				r.full = 0;
			for (auto& r : dDIFF)
				r.full = 0;
			for (auto& r : mAPF1)
				r.full = 0;
			for (auto& r : mAPF2)
				r.full = 0;
			for (auto& r : vIN)
				r = 0;
			vIIR = 0;
			vCOMB1 = 0;
			vCOMB2 = 0;
			vCOMB3 = 0;
			vCOMB4 = 0;
			vWALL = 0;
			vAPF1 = 0;
			vAPF2 = 0;
			m_Phase = 0;
			m_pos = 0;
		}

	private:
		static constexpr u32 NUM_TAPS = 39;

		SPUCore& m_SPU;

		std::array<SampleBuffer<s16, NUM_TAPS>, 2> m_ReverbIn{};
		std::array<SampleBuffer<s16, NUM_TAPS>, 2> m_ReverbOut{};

		s16 DownSample(AudioSample in);
		AudioSample UpSample(s16 in);

		s16 RD_RVB(s32 address, s32 offset = 0);
		void WR_RVB(s32 address, s16 sample);
		[[nodiscard]] u32 Offset(s32 offset) const;

		u32 m_Phase{0};
	};
} // namespace SPU
