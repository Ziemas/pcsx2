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

struct AdsrEntry
{
	s32 step;
	s32 fraction;

};

// ADSR implementation based on Dr. Hell's documentation
// Implementation based on PCRSXR's

static constexpr std::array<std::array<AdsrEntry, 128>, 2> InitADSR() // INIT ADSR
{
	std::array<std::array<AdsrEntry, 128>, 2> entries = {};

	for (int i = 0; i < 48; i++)
	{
		entries[0][i].step = (7 - (s32)(i & 3)) << (11 - (i >> 2));
		entries[1][i].step = (s32)((u32)(-8 + (s32)(i & 3)) << (11 - (i >> 2)));

		entries[0][i].fraction = 0;
		entries[1][i].fraction = 0;
	}

	for (int i = 48; i < 128; i++)
	{
        int denom = 1 << ((i >> 2) - 11);

		entries[0][i].step = (7 - (s32)(i & 3)) / denom;
		entries[1][i].step = (-8 + (s32)(i & 3)) / denom;

		entries[0][i].fraction = (7 - (s32)(i & 3)) % denom;
		entries[1][i].fraction = (-8 + (s32)(i & 3)) % denom;

		entries[0][i].fraction *= RateTable_denom / denom;
		entries[1][i].fraction *= RateTable_denom / denom;
	}

	return entries;
}

static constexpr std::array<std::array<AdsrEntry, 128>, 2> RateTable = InitADSR();


bool V_ADSR::Calculate()
{
	pxAssume(Phase != 0);

	if (Releasing && (Phase < 5))
		Phase = 5;

	switch (Phase)
	{
		case 1: // attack
			if (Value >= ADSR_MAX_VOL)
			{
				// Already maxed out.  Progress phase and nothing more:
				Phase++;
				break;
			}

			if (AttackMode == 1)
			{
				if (Value >= 0x6000)
				{
					Value += RateTable[0][AttackRate + 8].step;
					Fraction += RateTable[0][AttackRate + 8].fraction;
				}
				else
				{
					Value += RateTable[0][AttackRate].step;
					Fraction += RateTable[0][AttackRate].fraction;
				}
			}

			if (AttackMode == 0)
			{
				Value += RateTable[0][AttackRate].step;
				Fraction += RateTable[0][AttackRate].fraction;
			}

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
		case 2: // decay
		{
			Value += (RateTable[1][DecayRate * 4].step * Value) >> 15;

			Fraction += RateTable[1][DecayRate * 4].fraction;
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
			if (!SustainDecr)
			{
				if (SustainMode == 1)
				{
					if (Value >= 0x6000)
					{
						Value += RateTable[0][SustainRate + 8].step;
						Fraction += RateTable[0][SustainRate + 8].fraction;

					}
					else
					{
						Value += RateTable[0][SustainRate].step;
						Fraction += RateTable[0][SustainRate].fraction;
					}

				}

				if (SustainMode == 0)
				{
					Value += RateTable[0][SustainRate].step;
					Fraction += RateTable[0][SustainRate].fraction;

				}

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
				if (SustainMode == 1)
					Value += (RateTable[1][SustainRate].step * Value) >> 15;
				else
					Value += RateTable[1][SustainRate].step;

				Fraction += RateTable[1][SustainRate].fraction;
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

		case 5:              // release
			if (ReleaseMode) // exponential
				Value += (RateTable[1][ReleaseRate * 4].step * Value) >> 15;
			else // Linear
				Value += RateTable[1][ReleaseRate * 4].step;

			Fraction += RateTable[1][ReleaseRate * 4].fraction;
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
		{}
			//value += PsxRates[(Increment ^ 0x7f) - 0x18 + 32];
		else
		{}
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
