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

static constexpr s32 ADSR_MAX_VOL = 0x7fff;

s32 Envelope::next_step(s16 volume)
{
	if (cycles_left > 1)
	{
		cycles_left--;
		return 0;
	}

	// Need to handle negative phase for volume sweeps eventually

	cycles_left = 1 << std::max(0, shift - 11);
	s32 next_step = static_cast<s32>(step << std::max(0, 11 - shift));

	if (exponential && rising && volume > 0x6000)
		cycles_left <<= 2;

	if (exponential && !rising)
		next_step = (next_step * volume) >> 15;

	return next_step;
}

void V_ADSR::update()
{
	set_stage(stage);
}

void V_ADSR::set_stage(V_ADSR::Stage new_stage)
{
	// If the parameters change it seems we need to run the next step ASAP
	envelope.cycles_left = 0;
	switch (new_stage)
	{
		case V_ADSR::Stage::Attack:
			target = 0x7fff;
			envelope.exponential = ((adsr1 >> 15) & 1) ? true : false;
			envelope.negative_phase = false;
			envelope.rising = true;
			envelope.shift = (adsr1 >> 10) & 0x1f;
			envelope.step = (adsr1 >> 8) & 0x03;
			envelope.step = envelope.rising ? (7 - envelope.step) : (-8 + envelope.step);
			stage = new_stage;
			break;
		case V_ADSR::Stage::Decay:
			target = ((adsr1 & 0x0F) + 1) * 0x800;
			envelope.negative_phase = false;
			envelope.exponential = true;
			envelope.rising = false;
			envelope.shift = (adsr1 >> 4) & 0xf;
			envelope.step = -8;
			stage = new_stage;
			break;
		case V_ADSR::Stage::Sustain:
			target = 0;
			envelope.negative_phase = false;
			envelope.exponential = ((adsr2 >> 15) & 1) ? true : false;
			envelope.rising = ((adsr2 >> 14) & 1) ? false : true;
			envelope.shift = (adsr2 >> 8) & 0x1f;
			envelope.step = (adsr2 >> 6) & 0x03;
			envelope.step = envelope.rising ? (7 - envelope.step) : (-8 + envelope.step);
			stage = new_stage;
			break;
		case V_ADSR::Stage::Release:
			target = 0;
			envelope.negative_phase = false;
			envelope.exponential = ((adsr2 >> 5) & 1) ? true : false;
			envelope.rising = false;
			envelope.shift = adsr2 & 0x1f;
			envelope.step = -8;
			stage = new_stage;
			break;
		case V_ADSR::Stage::Stopped:
			stage = new_stage;
	}
}

void V_ADSR::advance()
{
	if (stage == V_ADSR::Stage::Stopped)
	{
		return;
	}

	int32_t step = envelope.next_step(volume);

	volume = static_cast<int16_t>(std::max(std::min(volume + step, 0x7fff), 0));

	if (stage != Stage::Sustain)
	{
		if ((envelope.rising && volume >= target) ||
			(!envelope.rising && volume <= target))
		{
			switch (stage)
			{
				case Stage::Attack:
					set_stage(Stage::Decay);
					break;
				case Stage::Decay:
					set_stage(Stage::Sustain);
					break;
				case Stage::Release:
					set_stage(Stage::Stopped);
					break;
				default:
					break;
			}
		}
	}
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

		if ((Mode & VOLFLAG_EXPONENTIAL) && (value >= 0x6000))
		{
		}
		//value += PsxRates[(Increment ^ 0x7f) - 0x18 + 32];
		else
		{
		}
		// linear / Pseudo below 75% (they're the same)
		//value += PsxRates[(Increment ^ 0x7f) - 0x10 + 32];

		if (value > ADSR_MAX_VOL) // wrapped around the "top"?
		{
			value = ADSR_MAX_VOL;
			Mode = 0; // disable slide
		}
	}

	Value = (Value < 0) ? -value : value;
}
