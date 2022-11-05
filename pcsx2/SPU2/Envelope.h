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
#include "common/Bitfield.h"

namespace SPU
{
	union ADSRReg
	{
		u32 bits;

		BitField<u32, u16, 16, 16> hi;
		BitField<u32, u16, 0, 16> lo;

		BitField<u32, bool, 31, 1> SustainExp;
		BitField<u32, bool, 30, 1> SustainDecr;
		BitField<u32, u8, 29, 1> Unused;
		BitField<u32, u8, 24, 5> SustainShift;
		BitField<u32, u8, 22, 2> SustainStep;
		BitField<u32, bool, 21, 1> ReleaseExp;
		BitField<u32, u8, 16, 5> ReleaseShift;

		BitField<u32, bool, 15, 1> AttackExp;
		BitField<u32, u8, 10, 5> AttackShift;
		BitField<u32, u8, 8, 2> AttackStep;
		BitField<u32, u8, 4, 4> DecayShift;
		BitField<u32, u8, 0, 4> SustainLevel;
	};

	union VolReg
	{
		u16 bits;

		BitField<u16, bool, 15, 1> EnableSweep;
		BitField<u16, bool, 14, 1> SweepExp;
		BitField<u16, bool, 13, 1> SweepDecrease;
		BitField<u16, bool, 12, 1> NegativePhase;
		BitField<u16, u8, 2, 5> SweepShift;
		BitField<u16, u8, 0, 2> SweepStep;
	};

	class Envelope
	{
	public:
		void Step()
		{
			u32 counter_inc = 0x8000 >> std::max(0, m_Shift - 11);
			s16 level_inc = static_cast<s16>(m_Step << std::max(0, 11 - m_Shift));

			if (m_Exp)
			{
				if (!m_Decrease && m_Level > 0x6000)
					counter_inc >>= 2;

				if (m_Decrease)
					level_inc = static_cast<s16>((level_inc * m_Level) >> 15);
			}

			m_Counter += counter_inc;

			if (m_Counter >= 0x8000)
			{
				m_Counter = 0;
				m_Level = std::clamp<s32>(m_Level + level_inc, 0, INT16_MAX);
			}
		}

	protected:
		u8 m_Shift{0};
		s8 m_Step{0};
		bool m_Inv{false};
		bool m_Exp{false};
		bool m_Decrease{false};

		u32 m_Counter{0};
		s32 m_Level{0};
	};

	class ADSR : Envelope
	{
	public:
		enum class Phase
		{
			Attack,
			Decay,
			Sustain,
			Release,
			Stopped,
		};

		void Run()
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
		void Attack()
		{
			m_Phase = Phase::Attack;
			m_Level = 0;
			m_Counter = 0;
			UpdateSettings();
		}
		void Release()
		{
			m_Phase = Phase::Release;
			m_Counter = 0;
			UpdateSettings();
		}
		void Stop()
		{
			m_Phase = Phase::Stopped;
			m_Level = 0;
		}
		[[nodiscard]] s16 Level() const { return static_cast<short>(m_Level); };
		void SetLevel(s16 value) { m_Level = value; }
		void UpdateSettings()
		{
			switch (m_Phase)
			{
				case Phase::Attack:
					m_Exp = m_Reg.AttackExp;
					m_Decrease = false;
					m_Shift = m_Reg.AttackShift;
					m_Step = static_cast<s8>(7 - m_Reg.AttackStep.GetValue());
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
					m_Step = m_Decrease ? static_cast<s8>(-8 + m_Reg.SustainStep.GetValue()) :
										  static_cast<s8>(7 - m_Reg.SustainStep.GetValue());
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
		ADSRReg m_Reg{0};
		[[nodiscard]] Phase GetPhase() const { return m_Phase; }

		void Reset()
		{
			m_Phase = Phase::Stopped;
			m_Target = 0;
			m_Counter = 0;
			m_Level = 0;
			m_Exp = false;
			m_Decrease = false;
			m_Inv = false;
			m_Step = 0;
			m_Shift = 0;
		}

	private:
		Phase m_Phase{Phase::Stopped};
		s32 m_Target{0};
	};

	class Volume : Envelope
	{
	public:
		void Run()
		{
			if (!m_Sweep.EnableSweep)
				return;

			Step();
		}
		void Set(u16 volume)
		{

			m_Sweep.bits = volume;

			if (!m_Sweep.EnableSweep)
			{
				m_Level = static_cast<s16>(m_Sweep.bits << 1);
				return;
			}

			m_Exp = m_Sweep.SweepExp;
			m_Decrease = m_Sweep.SweepDecrease;
			m_Shift = m_Sweep.SweepShift;
			m_Step = m_Decrease ? static_cast<s8>(-8 + m_Sweep.SweepStep.GetValue()) :
								  static_cast<s8>(7 - m_Sweep.SweepStep.GetValue());

			if (m_Exp && m_Decrease)
			{
				if (m_Sweep.NegativePhase)
					Console.WriteLn("Disqualified from inv");
				m_Inv = false;
			}
			else
			{
				m_Inv = m_Sweep.NegativePhase;
			}

			Console.WriteLn(Color_Red, "start sweep, e:%d d:%d sh:%d st:%d inv:%d", m_Exp, m_Decrease, m_Shift, m_Step, m_Inv);
			Console.WriteLn(Color_Red, "Current level %08x", m_Level);
		}
		[[nodiscard]] u16 Get() const { return m_Sweep.bits; }
		[[nodiscard]] s16 GetCurrent() const { return static_cast<s16>(m_Level); }

		void Reset()
		{
			m_Sweep.bits = 0;

			m_Counter = 0;
			m_Level = 0;
			m_Exp = false;
			m_Decrease = false;
			m_Inv = false;
			m_Step = 0;
			m_Shift = 0;
		}

	private:
		VolReg m_Sweep{0};
	};

	struct VolumePair
	{
		Volume left{};
		Volume right{};

		void Run()
		{
			left.Run();
			right.Run();
		}

		void Reset()
		{
			left.Reset();
			right.Reset();
		}
	};

} // namespace SPU
