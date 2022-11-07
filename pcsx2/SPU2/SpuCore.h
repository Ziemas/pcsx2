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

#include <immintrin.h>
#include "GS/GSVector.h"

namespace SPU
{
	class SPUCore
	{
		friend class Voice;

	public:
		SPUCore(u16* ram, u32 id)
			: m_Id(id)
			, m_RAM(ram)
		{
			m_share.RAM = m_RAM;
			m_share.SPU_ID = m_Id;
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
		static void TestIrq(u32 address);
		static void TestIrq(u32 start, u32 end);
		[[nodiscard]] s16 NoiseLevel() const { return m_Noise.Get(); }

		[[nodiscard]] AudioSample MemIn() const
		{
			auto displacement = (m_CurrentBuffer * BufSize) + m_BufPos + (m_Id * InBufOffset);
			auto laddress = static_cast<u32>(InBuf::MeminL) + displacement;
			auto raddress = static_cast<u32>(InBuf::MeminR) + displacement;
			return {static_cast<s16>(m_RAM[laddress]), static_cast<s16>(m_RAM[raddress])};
		}

		void Reset();

	private:
		static constexpr u32 NUM_VOICES = 24;
		static constexpr u32 SINER = 0;
		static constexpr u32 SINEL = 1;
		static constexpr u32 SINR = 2;
		static constexpr u32 SINL = 3;
		static constexpr u32 MINER = 4;
		static constexpr u32 MINEL = 5;
		static constexpr u32 MINR = 6;
		static constexpr u32 MINL = 7;
		static constexpr u32 MSNDER = 8;
		static constexpr u32 MSNDEL = 9;
		static constexpr u32 MSNDR = 10;
		static constexpr u32 MSNDL = 11;

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

		union SpdifMedia
		{
			u32 bits;

			BitField<u32, u16, 16, 16> hi;
			BitField<u32, u16, 0, 16> lo;

			// Output is copy protected
			BitField<u32, bool, 15, 1> Protect;

			// Output is a bitstream
			BitField<u32, bool, 1, 1> Bitstream;
		};

		union SpdifConfig
		{
			u16 bits;

			// ADMA output through spdif
			BitField<u16, bool, 8, 1> Bypass;

			// Route normal SPU output through spdif
			BitField<u16, bool, 5, 1> PCM;
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
			// if in dma mode this will be false while the fifo is filled
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

			BitField<u16, bool, 11, 1> VoiceL;
			BitField<u16, bool, 10, 1> VoiceR;
			BitField<u16, bool, 9, 1> VoiceWetL;
			BitField<u16, bool, 8, 1> VoiceWetR;
			BitField<u16, bool, 7, 1> MeminL;
			BitField<u16, bool, 6, 1> MeminR;
			BitField<u16, bool, 5, 1> MeminWetL;
			BitField<u16, bool, 4, 1> MeminWetR;
			BitField<u16, bool, 3, 1> SinL;
			BitField<u16, bool, 2, 1> SinR;
			BitField<u16, bool, 1, 1> SinWetL;
			BitField<u16, bool, 0, 1> SinWetR;
		};


		void RunADMA();
		[[nodiscard]] bool AdmaActive() const { return m_Id ? m_Adma.Core2.GetValue() : m_Adma.Core1.GetValue(); };
		void MemOut(OutBuf buffer, s16 value);

		u16* m_RAM;

		static std::array<Reg32, 2> m_IRQA;
		static std::array<AttrReg, 2> m_ATTR;
		static IrqStat m_IRQ;
		static constexpr GSVector8i OutBufVec = GSVector8i::cxpr(
			static_cast<s32>(OutBuf::SINL),
			static_cast<s32>(OutBuf::SINR),
			static_cast<s32>(OutBuf::Voice1),
			static_cast<s32>(OutBuf::Voice3),
			static_cast<s32>(OutBuf::MemOutL),
			static_cast<s32>(OutBuf::MemOutR),
			static_cast<s32>(OutBuf::MemOutEL),
			static_cast<s32>(OutBuf::MemOutER));

		SpdifConfig m_SPDIFConf{0};
		SpdifMedia m_SPDIFMedia{0};
		Status m_Stat{0};

		ADMA m_Adma{0};
		u16* m_MADR{nullptr};
		u32 m_DmaSize{0};

		u32 m_BufPos{0};
		u32 m_BufDmaCount{0};
		u32 m_CurrentBuffer{0};
		u32 m_CurBypassBuf{0};

		Reverb m_Reverb{*this};
		Noise m_Noise{};

		Reg32 m_TSA{0};
		u32 m_InternalTSA{0};

		FIFO<u16, 0x20> m_TransferFIFO{};

		VolumePair m_MVOL{};

		PlainVolReg m_EVOL{0};
		PlainVolReg m_AVOL{0};
		PlainVolReg m_BVOL{0};
		GSVector8i m_VOL{GSVector8i(INT16_MAX).broadcast16()};

		MMIX m_MMIX{0};
		GSVector8i m_vMMIX{};

		Reg32 m_VMIXL{0};
		Reg32 m_VMIXR{0};
		Reg32 m_VMIXEL{0};
		Reg32 m_VMIXER{0};

		VoiceVec m_vNON{};
		VoiceVec m_vPMON{};
		VoiceVec m_vVMIXL{};
		VoiceVec m_vVMIXR{};
		VoiceVec m_vVMIXEL{};
		VoiceVec m_vVMIXER{};

		SharedData m_share{};

		// clang-format off
		std::array<Voice, NUM_VOICES> m_voices = {{
			{m_share, 0},  {m_share, 1},  {m_share, 2},  {m_share, 3},
			{m_share, 4},  {m_share, 5},  {m_share, 6},  {m_share, 7},
			{m_share, 8},  {m_share, 9},  {m_share, 10}, {m_share, 11},
			{m_share, 12}, {m_share, 13}, {m_share, 14}, {m_share, 15},
			{m_share, 16}, {m_share, 17}, {m_share, 18}, {m_share, 19},
			{m_share, 20}, {m_share, 21}, {m_share, 22}, {m_share, 23},
		}};
		// clang-format on
	};

} // namespace SPU
