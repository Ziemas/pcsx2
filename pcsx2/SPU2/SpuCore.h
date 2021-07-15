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
#include "common/fifo.h"
#include "Reverb.h"
#include "Voice.h"
#include "Util.h"

namespace SPU
{
	static constexpr u32 NUM_VOICES = 24;

	class SPUCore
	{
	public:
		SPUCore(u16* ram, u32 id)
			: m_Id(id)
			, m_RAM(ram)
		{
		}

		u32 m_Id{0};

		std::pair<s16, s16> GenSample();

		void Write(u32 addr, u16 value);
		u16 Read(u32 addr);

		void WriteMem(u32 addr, u16 value);

		void DmaWrite(u16* madr, u32 size);
		void DmaRead(u16* madr, u32 size);
		u16 Ram(u32 address) { return m_RAM[address & 0xFFFFF]; }
		Voice& GetVoice(int n) { return m_voices[n]; }

	private:
		enum class TransferMode : u8
		{
			Stopped = 0,
			ManualWrite = 1,
			DMAWrite = 2,
			DMARead = 3,
		};

		union Attr
		{
			u16 bits;

			// iirc enable works like a reset switch here
			// driver flips enable on and expects DMA stuff to be reset
			BitField<u16, bool, 15, 1> Enable;
			BitField<u16, bool, 14, 1> OutputEnable;
			BitField<u16, u8, 8, 6> NoiseClock;
			BitField<u16, bool, 7, 1> EffectEnable;
			BitField<u16, bool, 6, 1> IRQEnable;
			BitField<u16, TransferMode, 4, 2> CurrentTransMode;
			// unknown if these do anything in ps2 mode
			BitField<u16, bool, 3, 1> ExtReverb;
			BitField<u16, bool, 2, 1> CDAReverb;
			BitField<u16, bool, 1, 1> EXTEnable;
			BitField<u16, bool, 0, 1> CDAEnable;
		};

		union Status
		{
			u16 bits;

			BitField<u16, bool, 10, 1> DMABusy;
			BitField<u16, bool, 7, 1> DMAReady;
		};

		union ADMA
		{
			u16 bits;

			BitField<u16, bool, 2, 1> ReadMode;
			// Separate bits for each core
			// despite the fact that they're separate regs... (aifai)
			BitField<u16, bool, 1, 1> Core2;
			BitField<u16, bool, 0, 1> Core1;
		};

		union MMIX
		{
			u16 bits;

			BitField<u16, bool, 11, 1> VoiceR;
			BitField<u16, bool, 10, 1> VoiceL;
			BitField<u16, bool, 9, 1> VoiceWetR;
			BitField<u16, bool, 8, 1> VoiceWetL;
			BitField<u16, bool, 7, 1> MeminR;
			BitField<u16, bool, 6, 1> MeminL;
			BitField<u16, bool, 5, 1> MeminWetR;
			BitField<u16, bool, 4, 1> MeminWetL;
			BitField<u16, bool, 3, 1> SinR;
			BitField<u16, bool, 2, 1> SinL;
			BitField<u16, bool, 1, 1> SinWetR;
			BitField<u16, bool, 0, 1> SinWetL;
		};


		u16* m_RAM;

		Attr m_Attr{0};
		Status m_Stat{0};

		ADMA m_Adma{0};

		Reverb m_Reverb{*this};

		Reg32 m_TSA{0};
		u32 m_InternalTSA{0};

		FIFO<u16, 0x20> m_TransferFIFO{};

		VolumePair m_MVOL{};

		PlainVolReg m_EVOL{0};
        PlainVolReg m_AVOL{0};
        PlainVolReg m_BVOL{0};

		//u32 m_KeyOn{0};
		//u32 m_KeyOff{0};
		//u32 m_PitchMod{0};
		//u32 m_Noise{0};

		MMIX m_MMIX{0};
		Reg32 m_VMIXL{0};
		Reg32 m_VMIXR{0};
		Reg32 m_VMIXEL{0};
		Reg32 m_VMIXER{0};

		// clang-format off
		std::array<Voice, NUM_VOICES> m_voices = {{
			{*this, 0},  {*this, 1},  {*this, 2},  {*this, 3},
			{*this, 4},  {*this, 5},  {*this, 6},  {*this, 7},
			{*this, 8},  {*this, 9},  {*this, 10}, {*this, 11},
			{*this, 12}, {*this, 13}, {*this, 14}, {*this, 15},
			{*this, 16}, {*this, 17}, {*this, 18}, {*this, 19},
			{*this, 20}, {*this, 21}, {*this, 22}, {*this, 23},
		}};
		// clang-format on
	};

} // namespace SPU
