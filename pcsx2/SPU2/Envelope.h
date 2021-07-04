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
		BitField<u16, s8, 0, 2> SweepStep;
	};

	class Envelope
	{
	public:
		void Step();

	protected:
		u8 m_Shift{0};
		s8 m_Step{0};
		bool m_Exp{0};
		bool m_Decrease{0};

		u32 m_Counter{0};
		s32 m_Level{0};
	};

	class ADSR : Envelope
	{
		enum class Phase
		{
			Attack,
			Decay,
			Sustain,
			Release,
			Stopped,
		};

	public:
		void Run();
		void Attack();
		void Release();
		void Stop();
		s16 Level();
		void UpdateSettings();
		ADSRReg m_Reg{0};

	private:
		Phase m_Phase{Phase::Stopped};
		u32 m_Target{0};
	};

	class Volume : Envelope
	{
	public:
		void Run();
		void Set(u16 volume);
		s16 Get();

	private:
		VolReg m_Sweep{0};
		s16 m_Vol{0};
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
	};

} // namespace SPU
