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
#include <algorithm>
#include <array>

namespace SPU
{
	void Envelope::Step()
	{
		// arbitrary number of bits, this is probably incorrect for the
		// "reserved" and infinite duration values
		// test hw or copy mednafen instead?
		u32 cStep = 0x800000;

		s32 shift = m_Shift - 11;
		if (shift > 0)
			cStep >>= shift;

		s16 step = m_Step << std::max<s16>(0, 11 - m_Shift);

		if (m_Exp)
		{
			if (!m_Decrease && m_Level > 0x6000)
				cStep >>= 2;

			if (m_Decrease)
				step = (step * m_Level) >> 15;
		}

		m_Counter += cStep;

		if (m_Counter >= 0x800000)
		{
			m_Counter = 0;
			m_Level = std::clamp<s32>(m_Level + step, 0, 0x7FFF);
		}
	}

	void ADSR::Run()
	{
		// Let's not waste time calculating silent voices
		if (m_Phase == Phase::Stopped)
			return;

		Step();

		if (m_Phase == Phase::Sustain)
			return;

		if ((!m_Decrease && m_Level >= m_Target) || (m_Decrease && m_Level <= m_Target))
		{
			switch (m_Phase)
			{
				case Phase::Attack:
					m_Phase = Phase::Decay;
					break;
				case Phase::Decay:
					m_Phase = Phase::Sustain;
					break;
				case Phase::Release:
					m_Phase = Phase::Stopped;
					break;
				default:
					break;
			}

			UpdateSettings();
		}
	}

	void ADSR::UpdateSettings()
	{
		switch (m_Phase)
		{
			case Phase::Attack:
				m_Exp = m_Reg.AttackExp;
				m_Decrease = false;
				m_Shift = m_Reg.AttackShift;
				m_Step = 7 - m_Reg.AttackStep.GetValue();
				m_Target = 0x7FFF;
				break;
			case Phase::Decay:
				m_Exp = true;
				m_Decrease = true;
				m_Shift = m_Reg.DecayShift;
				m_Step = -8;
				m_Target = (m_Reg.SustainLevel.GetValue() + 1) << 11;
				break;
			case Phase::Sustain:
				m_Exp = m_Reg.SustainExp;
				m_Decrease = m_Reg.SustainDecr;
				m_Shift = m_Reg.SustainShift;
				m_Step = m_Decrease ? (-8 + m_Reg.SustainStep.GetValue()) : (7 - m_Reg.SustainStep.GetValue());
				m_Target = 0; // unused for sustain
				break;
			case Phase::Release:
				m_Exp = m_Reg.ReleaseExp;
				m_Decrease = true;
				m_Shift = m_Reg.ReleaseShift;
				m_Step = -8;
				m_Target = 0;
				break;
			default:
				break;
		}
	}

	void ADSR::Attack()
	{
		m_Phase = Phase::Attack;
		m_Level = 0;
		m_Counter = 0;
		UpdateSettings();
	}

	void ADSR::Release()
	{
		m_Phase = Phase::Release;
		m_Counter = 0;
		UpdateSettings();
	}

	void ADSR::Stop()
	{
		m_Phase = Phase::Stopped;
		m_Level = 0;
	}

	u32 ADSR::Level()
	{
		return m_Level;
	}

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
