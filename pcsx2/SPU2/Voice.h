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

#include "gcem.hpp"
#include "common/Pcsx2Types.h"
#include "common/Bitfield.h"
#include "common/fifo.h"
#include "Envelope.h"
#include "Util.h"

namespace SPU
{
	static constexpr std::array<std::array<s16, 4>, 256> gaussianConstructTable()
	{
		std::array<std::array<s16, 4>, 256> result = {};
		double table[512] = {};
		for (u32 n = 0; n < 512; n++)
		{
			double k = 0.5 + n;
			auto s = static_cast<double>((gcem::sin(GCEM_PI * k * 2.048 / 1024)));
			auto t = static_cast<double>((gcem::cos(GCEM_PI * k * 2.000 / 1023) - 1) * 0.50);
			auto u = static_cast<double>((gcem::cos(GCEM_PI * k * 4.000 / 1023) - 1) * 0.08);
			double r = s * (t + u + 1.0) / k;
			table[511 - n] = r;
		}
		double sum = 0.0;
		for (double n : table)
			sum += n;
		double scale = 0x7f80 * 128 / sum;
		for (double& n : table)
			n *= scale;
		for (u32 phase = 0; phase < 256; phase++)
		{
			double phase_sum = 0.0;
			phase_sum += table[phase + 0];
			phase_sum += table[phase + 256];
			phase_sum += table[511 - phase];
			phase_sum += table[255 - phase];
			double diff = (phase_sum - 0x7f80) / 4;
			result[255 - phase][0] = static_cast<s16>(gcem::round(table[phase + 0] - diff));
			result[255 - phase][1] = static_cast<s16>(gcem::round(table[phase + 256] - diff));
			result[255 - phase][2] = static_cast<s16>(gcem::round(table[511 - phase] - diff));
			result[255 - phase][3] = static_cast<s16>(gcem::round(table[255 - phase] - diff));
		}

		return result;
	}

	static constexpr std::array<std::array<s16, 4>, 256> gaussianTable = gaussianConstructTable();

	struct SharedData
	{
		u32 SPU_ID;
		u16* RAM{nullptr};
		Reg32 KeyOn{0};
		Reg32 KeyOff{0};
		Reg32 PitchMod{0};
		Reg32 ENDX{0};
		Reg32 NON{0};
		AddrVec vNAX{};
		VoiceVec Pitch{};
		VoiceVec Counter{};
		VoiceVec OUTX{};
		VoiceVec VOLL{};
		VoiceVec VOLR{};
		VoiceVec ENVX{};
	};

	class Voice
	{
	public:
		Voice(SharedData& share, u32 id)
			: m_Share(share)
			, m_Id(id)
		{
		}

		void ProcessKonKoff()
		{
			if (m_Share.KeyOff.full & (1 << m_Id))
			{
				m_ADSR.Release();
				//Console.WriteLn("SPU[%d]:VOICE[%d] Key Off", m_SPU.m_Id, m_Id);
			}
			if (m_Share.KeyOn.full & (1 << m_Id))
			{
				m_Share.vNAX.arr[m_Id].full = m_SSA.full;
				m_Share.vNAX.arr[m_Id].full++;

				m_Share.ENDX.full &= ~(1 << m_Id);
				m_ADSR.Attack();
				m_Share.Counter.uarr[m_Id] = 0;
				m_DecodeHist1 = 0;
				m_DecodeHist2 = 0;
				m_Buffer = {};
				m_Rpos = 0;
				m_Wpos = 0;
				m_CustomLoop = false;
				//Console.WriteLn("SPU[%d]:VOICE[%d] Key On, SSA %08x", m_SPU.m_Id, m_Id, m_SSA);
			}
		}

		void DecodeSamples()
		{
			// The block header (and thus LSA) updates every spu cycle
			m_CurHeader.bits = m_Share.RAM[(m_Share.vNAX.arr[m_Id].full & ~0x7) & 0xfffff];
			if (m_CurHeader.LoopStart && !m_CustomLoop)
				m_LSA.full = m_Share.vNAX.arr[m_Id].full & ~0x7;

			// This doesn't exactly match the real behaviour,
			// it seems to initially decode a bigger chunk
			// and then decode more data after a bit has drained
			if (Size() >= 16)
			{
				// sufficient data buffered
				return;
			}

			// Skip decoding for stopped voices.
			// technically not safe i think, envx could be written
			// directly to make it audible again.
			// But it's too good a speed boost to let go.
			if (m_ADSR.GetPhase() == ADSR::Phase::Stopped)
			{
				PushSkipN(4);
			}
			else
			{
				u32 data = m_Share.RAM[m_Share.vNAX.arr[m_Id].full & 0xfffff];
				for (int i = 0; i < 4; i++)
				{
					s32 sample = (s16)((data & 0xF) << 12);
					sample >>= m_CurHeader.Shift.GetValue();

					// TODO do the right thing for invalid shift/filter values
					sample += (adpcm_coefs_i[m_CurHeader.Filter.GetValue()][0] * m_DecodeHist1) >> 6;
					sample += (adpcm_coefs_i[m_CurHeader.Filter.GetValue()][1] * m_DecodeHist2) >> 6;

					// We do get overflow here otherwise, should we?
					sample = std::clamp<s32>(sample, INT16_MIN, INT16_MAX);

					m_DecodeHist2 = m_DecodeHist1;
					m_DecodeHist1 = static_cast<s16>(sample);

					Push(static_cast<s16>(sample));
					data >>= 4;
				}
			}

			m_Share.vNAX.arr[m_Id].full++;

			if ((m_Share.vNAX.arr[m_Id].full & 0x7) == 0)
			{
				if (m_CurHeader.LoopEnd)
				{
					m_Share.vNAX.arr[m_Id].full = m_LSA.full;
					m_Share.ENDX.full |= (1 << m_Id);

					if (!m_CurHeader.LoopRepeat)
					{
						// Need to inhibit stopping here in noise is on
						// seems to result in the right thing but would like to verify
						if ((m_Share.NON.full & (1 << m_Id)) == 0U)
							m_ADSR.Stop();
					}
				}

				m_Share.vNAX.arr[m_Id].full++;
			}
		};

		void UpdateCounter()
		{
			s32 step = m_Share.Pitch.uarr[m_Id];
			if ((m_Share.PitchMod.full & (1 << m_Id)) != 0U)
			{
				s32 factor = m_Share.OUTX.uarr[m_Id];
				step = (step << 16) >> 16;
				step = (step * factor) >> 15;
				step &= 0xFFFF;
			}

			step = std::min(step, 0x3FFF);
			m_Share.Counter.uarr[m_Id] += step;
			PopN(m_Share.Counter.uarr[m_Id] >> 12);
			m_Share.Counter.uarr[m_Id] &= 0xFFF;

			m_Share.ENVX.arr[m_Id] = m_ADSR.Level();
			m_Share.VOLL.arr[m_Id] = m_Volume.left.GetCurrent();
			m_Share.VOLR.arr[m_Id] = m_Volume.right.GetCurrent();

			m_ADSR.Run();
			m_Volume.Run();
		};

		[[nodiscard]] u16 Read(u32 addr) const
		{
			switch (addr)
			{
				case 0:
					return m_Volume.left.Get();
				case 2:
					return m_Volume.right.Get();
				case 4:
					return m_Share.Pitch.uarr[m_Id];
				case 6:
					return m_ADSR.m_Reg.lo.GetValue();
				case 8:
					return m_ADSR.m_Reg.hi.GetValue();
				case 10:
					return m_ADSR.Level();
				case 12:
					return m_Volume.left.GetCurrent();
				case 14:
					return m_Volume.right.GetCurrent();
				default:
					Console.WriteLn("UNHANDLED SPU[%d]:VOICE[%d] READ ---- <- %04x", m_Share.SPU_ID, m_Id, addr);
					pxAssertMsg(false, "Unhandled SPU Write");
					return 0;
			}
		}

		void Write(u32 addr, u16 value)
		{
			switch (addr)
			{
				case 0:
					m_Volume.left.Set(value);
					return;
				case 2:
					m_Volume.right.Set(value);
					return;
				case 4:
					m_Share.Pitch.uarr[m_Id] = value;
					return;
				case 6:
					m_ADSR.m_Reg.lo = value;
					m_ADSR.UpdateSettings();
					return;
				case 8:
					m_ADSR.m_Reg.hi = value;
					m_ADSR.UpdateSettings();
					return;
				case 10:
					// writeable envx
					m_ADSR.SetLevel(static_cast<s16>(value));
					return;
				case 12:
				case 14:
					// These two are not writeable though
					return;
				default:
					Console.WriteLn("UNHANDLED SPU[%d]:VOICE[%d] WRITE %04x -> %04x", m_Share.SPU_ID, m_Id, value, addr);
					pxAssertMsg(false, "Unhandled SPU Write");
			}
		}

		// The new (for SPU2) full addr regs are a separate range
		[[nodiscard]] u16 ReadAddr(u32 addr) const
		{
			switch (addr)
			{
				case 0:
					return m_SSA.hi.GetValue();
				case 2:
					return m_SSA.lo.GetValue();
				case 4:
					return m_LSA.hi.GetValue();
				case 6:
					return m_LSA.lo.GetValue();
				case 8:
					return m_Share.vNAX.arr[m_Id].hi.GetValue();
				case 10:
					return m_Share.vNAX.arr[m_Id].lo.GetValue();
				default:
					Console.WriteLn("UNHANDLED SPU[%d]:VOICE[%d] ReadAddr ---- <- %04x", m_Share.SPU_ID, m_Id, addr);
					pxAssertMsg(false, "Unhandled SPU Write");
					return 0;
			}
		}
		void WriteAddr(u32 addr, u16 value)
		{
			switch (addr)
			{
				case 0:
					m_SSA.hi = value & 0xF;
					return;
				case 2:
					m_SSA.lo = value;
					return;
				case 4:
					m_LSA.hi = value & 0xF;
					m_CustomLoop = true;
					return;
				case 6:
					m_LSA.lo = value;
					m_CustomLoop = true;
					return;
				case 8:
					m_Share.vNAX.arr[m_Id].hi = value & 0xF;
					return;
				case 10:
					m_Share.vNAX.arr[m_Id].lo = value;
					return;
				default:
					Console.WriteLn("UNHANDLED SPU[%d]:VOICE[%d] WriteAddr %04x -> %04x", m_Share.SPU_ID, m_Id, value, addr);
					pxAssertMsg(false, "Unhandled SPU Write");
			}
		}

		void Reset()
		{
			m_DecodeHist1 = 0;
			m_DecodeHist2 = 0;
			m_SSA.full = 0;
			m_LSA.full = 0;
			m_CustomLoop = false;
			m_CurHeader.bits = 0;
			m_ADSR.Reset();
			m_Volume.Reset();

			m_Buffer.fill({});
			m_Rpos = 0;
			m_Wpos = 0;
		}

		void PopN(size_t n) { m_Rpos += n; }
		void Push(s16 val)
		{
			m_Buffer[(m_Wpos & 0x1f) | 0x0] = val;
			m_Buffer[(m_Wpos & 0x1f) | 0x20] = val;
			m_Wpos++;
		}

		void PushSkipN(size_t n)
		{
			m_Wpos += n;
		}

		u16 Size() { return static_cast<u16>(m_Wpos - m_Rpos); }

		s16* Get()
		{
			return &m_Buffer[m_Rpos & 0x1f];
		}

	private:
		union ADPCMHeader
		{
			u16 bits;
			BitField<u16, bool, 10, 1> LoopStart;
			BitField<u16, bool, 9, 1> LoopRepeat;
			BitField<u16, bool, 8, 1> LoopEnd;
			BitField<u16, u8, 4, 3> Filter;
			BitField<u16, u8, 0, 4> Shift;
		};

		// Integer math version of ps-adpcm coefs
		static constexpr s32 adpcm_coefs_i[5][2] = {
			{0, 0},
			{60, 0},
			{115, -52},
			{98, -55},
			{122, -60},
		};

		alignas(32) std::array<s16, 0x20 << 1> m_Buffer{};

		u16 m_Rpos{0};
		u16 m_Wpos{0};

		SharedData& m_Share;
		s32 m_Id{0};

		s16 m_DecodeHist1{0};
		s16 m_DecodeHist2{0};

		Reg32 m_SSA{0};
		Reg32 m_LSA{0};
		bool m_CustomLoop{false};

		ADPCMHeader m_CurHeader{};

		ADSR m_ADSR{};
		VolumePair m_Volume{};
	};
} // namespace SPU
