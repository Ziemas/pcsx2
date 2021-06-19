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

		u32 shift = m_Shift - 11;
		if (shift > 0)
		{
			cStep >>= shift;
		}

		u32 step = m_Step << std::max<u32>(0, 11 - m_Shift);

		if (m_Exp)
		{
			if (!m_Decrease && m_Level > 0x6000)
				cStep >>= 2;

			if (m_Decrease)
				step = (step * m_Level) >> 15;
		}

		if (m_Counter >= 0x800000)
		{
			m_Counter = 0;
			m_Level = std::clamp<u32>(m_Level + step, 0, 0x7FFF);
		}

		m_Counter += cStep;
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
				m_Step = m_Reg.AttackStep;
				m_Target = 0x7FFF;
				break;
		}
	}

	void ADSR::Attack()
	{
		m_Phase = Phase::Attack;
		UpdateSettings();
	}

	void ADSR::Release()
	{
		m_Phase = Phase::Release;
		UpdateSettings();
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
