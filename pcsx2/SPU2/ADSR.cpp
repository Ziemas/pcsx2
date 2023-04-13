/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "PrecompiledHeader.h"
#include "Global.h"
#include "common/Assertions.h"

void V_ADSR::NewPhase(u8 phase)
{
	Phase = phase;

	switch (phase)
	{
		case 1: // attack
		{
			Value = 0;
			Decrease = false;
			Exp = AttackMode;
			Target = 0x7fff;
			Shift = AttackShift;
			Step = 7 - AttackStep;
		}
		break;

		case 2: // decay
		{
			Decrease = true;
			Exp = true;
			Target = (SustainLevel + 1) << 11;
			Shift = DecayShift;
			Step = -8;
		}
		break;

		case 3: // sustain
		{
			Decrease = SustainDir;
			Exp = SustainMode;
			Target = 0;
			Shift = SustainShift;
			Step = Decrease ? -8 + SustainStep : 7 - SustainStep;
		}
		break;

		case 4: // release
		{
			Decrease = true;
			Exp = ReleaseMode;
			Target = 0;
			Shift = ReleaseShift;
			Step = -8;
		}
		break;

		case 5: // release end
		{
			Value = 0;
		}
		break;

			jNO_DEFAULT
	}
}

bool V_ADSR::Calculate()
{
	pxAssume(Phase != 0);

	if (Phase == 5)
	{
		return false;
	}

	u32 counter_inc = 0x8000 >> std::max(0, Shift - 11);
	s16 value_inc = static_cast<s16>(Step << std::max(0, 11 - Shift));

	Counter += counter_inc;

	if (Counter >= 0x8000)
	{
		Counter = 0;
		Value = std::clamp<s32>(Value + value_inc, 0, INT16_MAX);
	}

	// Sustain until key off
	if (Phase == 3)
	{
		return true;
	}

	// Incrument phase if target reached
	if ((!Decrease && Value >= Target) || (Decrease && Value <= Target))
	{
		NewPhase(Phase + 1);
	}

	// returns true if the voice is active, or false if it's stopping.
	return Phase != 5;
}

/////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////
//                                                                                     //

#define VOLFLAG_REVERSE_PHASE (1ul << 0)
#define VOLFLAG_DECREMENT (1ul << 1)
#define VOLFLAG_EXPONENTIAL (1ul << 2)
#define VOLFLAG_SLIDE_ENABLE (1ul << 3)

void V_VolumeSlide::Update()
{
	if (!(Mode & VOLFLAG_SLIDE_ENABLE))
		return;

	// Volume slides use the same basic logic as ADSR, but simplified (single-stage
	// instead of multi-stage)

	if (Increment == 0x7f)
		return;

	s32 value = abs(Value);

	if (Mode & VOLFLAG_DECREMENT)
	{
		// Decrement

		if (Mode & VOLFLAG_EXPONENTIAL)
		{
			//u32 off = InvExpOffsets[(value >> 28) & 7];
			//value -= PsxRates[(Increment ^ 0x7f) - 0x1b + off + 32];
		}
		else
		{
			//value -= PsxRates[(Increment ^ 0x7f) - 0xf + 32];
		}

		if (value < 0)
		{
			value = 0;
			Mode = 0; // disable slide
		}
	}
	else
	{
		// Increment
		// Pseudo-exponential increments, as done by the SPU2 (really!)
		// Above 75% slides slow, below 75% slides fast.  It's exponential, pseudo'ly speaking.

		if ((Mode & VOLFLAG_EXPONENTIAL) && (value >= 0x60000000))
		{
			//value += PsxRates[(Increment ^ 0x7f) - 0x18 + 32];
		}
		else
		{
			// linear / Pseudo below 75% (they're the same)
			//value += PsxRates[(Increment ^ 0x7f) - 0x10 + 32];
		}

		if (value < 0) // wrapped around the "top"?
		{
			value = 0x7fff;
			Mode = 0; // disable slide
		}
	}

	Value = (Value < 0) ? -value : value;
}
