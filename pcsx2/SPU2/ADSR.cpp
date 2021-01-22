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

//static const s32 ADSR_MAX_VOL = 0x7fffffff;
static constexpr s32 ADSR_MAX_VOL = 0x7fff;

static constexpr int RateTable_denom = 1 << (((4 * 32) >> 2) - 11);

// ADSR implementation based on Dr. Hell's documentation
// Fraction stuff borrowed from PCSXR

static std::pair<s32, s32> Step(bool rising, u32 rate, bool exp, s32 value)
{
	u32 real_rate = rate;
	s32 step = 0;
	s32 fraction = 0;
	int denom = 1 << ((rate >> 2) - 11);

	if (rising)
	{
		if (exp && value > 0x6000)
			real_rate += 8;

		if (rate < 48)
		{
			step = (7 - (s32)(real_rate & 3)) << (11 - (real_rate >> 2));
		}
		else
		{
			step = (7 - (s32)(real_rate & 3)) / denom;
			fraction = (7 - (s32)(real_rate & 3)) % denom;
			fraction *= RateTable_denom / denom;
		}
	}
	else
	{
		if (rate < 48)
		{
			step = (s32)((u32)(-8 + (s32)(real_rate & 3)) << (11 - (real_rate >> 2)));
		}
		else
		{
			step = (-8 + (s32)(real_rate & 3)) / denom;
			if (exp)
			{
				fraction = (((-8 + (s32)(real_rate & 3)) * real_rate) >> 15) % denom;
				fraction *= RateTable_denom / denom;
			}
			else
			{
				fraction = (-8 + (s32)(real_rate & 3)) % denom;
				fraction *= RateTable_denom / denom;
			}
		}
	}

	return std::make_pair(step, fraction);
}


bool V_ADSR::Calculate()
{
	pxAssume(Phase != 0);

	if (Releasing && (Phase < 5))
		Phase = 5;

	switch (Phase)
	{
		case 1: // attack
		{
			if (Value >= ADSR_MAX_VOL)
			{
				// Already maxed out.  Progress phase and nothing more:
				Phase++;
				break;
			}

			auto [v, f] = Step(true, AttackRate, AttackMode, Value);

			Value += v;
			Fraction += f;

			if (Fraction >= RateTable_denom)
			{
				Fraction -= RateTable_denom;
				Value++;
			}

			if (Value < 0 || Value >= 0x8000)
			{
				// We hit the ceiling.
				Phase++;
				Value = ADSR_MAX_VOL;
				Fraction = RateTable_denom;
			}

			break;
		}
		case 2: // decay
		{
			auto [v, f] = Step(false, DecayRate * 4, true, Value);

			Value += v;
			Fraction += f;

			if (Fraction < 0)
			{
				Fraction += RateTable_denom;
				Value--;
			}

			if (Value < 0)
			{
				Value = 0;
				Fraction = 0;
			}

			if (((Value >> 11) & 0xf) <= SustainLevel)
			{
				Phase++;
			}

			break;
		}
		case 3: // sustain
		{
			auto [v, f] = Step(!SustainDecr, SustainRate, SustainMode, Value);

			Value += v;
			Fraction += f;

			if (!SustainDecr)
			{
				if (Fraction >= RateTable_denom)
				{
					Fraction -= RateTable_denom;
					Value++;
				}

				if (Value >= 0x8000)
				{
					Value = ADSR_MAX_VOL;
					Fraction = RateTable_denom;
					Phase++;
				}
			}
			else
			{
				if (Fraction < 0)
				{
					Fraction += RateTable_denom;
					Value--;
				}

				if (Value < 0)
				{
					Value = 0;
					Fraction = 0;
					Phase++;
				}
			}
		}
		break;

		case 4: // sustain end
			Value = (SustainDecr) ? 0 : ADSR_MAX_VOL;
			if (Value == 0)
				Phase = 6;
			break;

		case 5: // release
		{
			auto [v, f] = Step(false, ReleaseRate * 4, ReleaseMode, Value);

			Value += v;
			Fraction += f;

			if (Fraction < 0)
			{
				Fraction += RateTable_denom;
				Value--;
			}

			if (Value <= 0)
			{
				Value = 0;
				Fraction = 0;
				Phase++;
			}
			break;
		}
		case 6: // release end
			Value = 0;
			break;

			jNO_DEFAULT
	}

	// returns true if the voice is active, or false if it's stopping.
	return Phase != 6;
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
			//value -= PsxRates[(Increment ^ 0x7f) - 0xf + 32];

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
		}
		//value += PsxRates[(Increment ^ 0x7f) - 0x18 + 32];
		else
		{
		}
		// linear / Pseudo below 75% (they're the same)
		//value += PsxRates[(Increment ^ 0x7f) - 0x10 + 32];

		if (value < 0) // wrapped around the "top"?
		{
			value = 0x7fffffff;
			Mode = 0; // disable slide
		}
	}

	Value = (Value < 0) ? -value : value;
}
