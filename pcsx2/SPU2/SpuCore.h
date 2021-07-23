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
#include "Noise.h"
#include "Reverb.h"
#include "Voice.h"
#include "Util.h"

namespace SPU
{
	static constexpr u32 NUM_VOICES = 24;

	class SPUCore
	{
	public:
		enum class OutBuf
		{
			SINL = 0x0,
			SINR = 0x200,
			Voice1 = 0x400,
			Voice3 = 0x600,
			MemOutL = 0x1000,
			MemOutR = 0x1200,
			MemOutEL = 0x1400,
			MemOutER = 0x1600,
		};

		SPUCore(u16* ram, u32 id)
			: m_Id(id)
			, m_RAM(ram)
		{
		}

		u32 m_Id{0};

		AudioSample GenSample(AudioSample input);

		void Write(u32 addr, u16 value);
		u16 Read(u32 addr);

		void WriteMem(u32 addr, u16 value);

		void RunDma();
		void DmaWrite(u16* madr, u32 size);
		void DmaRead(u16* madr, u32 size);
		u16 Ram(u32 address) { return m_RAM[address & 0xFFFFF]; }
		Voice& GetVoice(int n) { return m_voices[n]; }
		void MemOut(OutBuf buffer, s16 value);
		static void TestIrq(u32 address);
		static void TestIrq(u32 start, u32 end);
		[[nodiscard]] s16 NoiseLevel() const { return m_Noise.Get(); }

		void Reset();

	private:
		static constexpr u32 DmaFifoSize = 0x20;
		static constexpr u32 BufSize = 0x100;
		static constexpr u32 OutBufCoreOffset = 0x800;
		static constexpr u32 InBufOffset = 0x400;
		enum class InBuf
		{
			MeminL = 0x2000,
			MeminR = 0x2200,
		};

		enum class TransferMode : u8
		{
			Stopped = 0,
			ManualWrite = 1,
			DMAWrite = 2,
			DMARead = 3,
		};

		union AttrReg
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

		union IrqStat
		{
			u32 bits;

			BitField<u32, bool, 4, 1> BufferHalf;
			BitField<u32, bool, 3, 1> CauseC1;
			BitField<u32, bool, 2, 1> CauseC0;
		};

		union Status
		{
			u16 bits;

			// These two bits match PS1, maybe check others
			// although these are the only two libsd cares about

			// True while transfer in progress?
			// not observable in our implementation?
			BitField<u16, bool, 10, 1> DMABusy;
			// if dma mode this will be false while the fifo is filled
			// and true when drained
			// libsd's dma irq handler waits for the buffer to finish draining
			BitField<u16, bool, 7, 1> DMARequest;
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


		void RunADMA();
		[[nodiscard]] bool AdmaActive() const { return m_Id ? m_Adma.Core2.GetValue() : m_Adma.Core1.GetValue(); };

		u16* m_RAM;

		static std::array<Reg32, 2> m_IRQA;
		static std::array<AttrReg, 2> m_ATTR;
		static IrqStat m_IRQ;

		Status m_Stat{0};

		ADMA m_Adma{0};
		u16* m_MADR{nullptr};
		u32 m_DmaSize{0};

		u32 m_BufPos{0};
		u32 m_CurrentBuffer{0};

		Reverb m_Reverb{*this};
		Noise m_Noise{};

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
