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
		std::array<double, 512> table = {};
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

	static constexpr s32 NUM_VOICES = 48;

	union AddrVec
	{
		std::array<Reg32, NUM_VOICES> arr;
		std::array<GSVector8i, 6> vec;

		void Reset()
		{
			for (int i = 0; i < NUM_VOICES; i++)
			{
				arr[i].full = 0;
			}
		}
	};

	union VoiceVec
	{
		std::array<s16, NUM_VOICES> arr;
		std::array<u16, NUM_VOICES> uarr;

		// lets fit in 3 of these so we have room to write
		// outx with an offset
		std::array<GSVector8i, 4> vec;

		void Reset()
		{
			for (int i = 0; i < NUM_VOICES; i++)
			{
				arr[i] = 0;
			}
		}
	};

	struct Decoder
	{
		void Reset()
		{
			m_DecodeHist1 = 0;
			m_DecodeHist2 = 0;
			m_SSA.full = 0;
			m_LSA.full = 0;
			m_CustomLoop = false;
			m_CurHeader.bits = 0;

			m_Buffer.fill({});
			m_Wpos = 0;
		}

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

		u16 m_Wpos{0};
		s16 m_DecodeHist1{0};
		s16 m_DecodeHist2{0};

		Reg32 m_SSA{0};
		Reg32 m_LSA{0};
		bool m_CustomLoop{false};

		ADPCMHeader m_CurHeader{};
	};

	struct Voices
	{
		Voices(u16* ram)
			: RAM(ram){};
		std::array<Decoder, NUM_VOICES> decoder;
		std::array<ADSR, NUM_VOICES> adsr;
		std::array<VolumePair, NUM_VOICES> volume;
		u16* RAM{nullptr};
		Reg32 KeyOn{0};
		Reg32 KeyOff{0};
		Reg32 PitchMod{0};
		Reg32 ENDX{0};
		Reg32 NON{0};
		AddrVec vNAX{};
		VoiceVec Pitch{};
		VoiceVec Counter{};
		VoiceVec SamplePos{};
		VoiceVec OUTX{};
		VoiceVec VOLL{};
		VoiceVec VOLR{};
		VoiceVec ENVX{};
		VoiceVec vNON{};
		VoiceVec vPMON{};
		VoiceVec vVMIXL{};
		VoiceVec vVMIXR{};
		VoiceVec vVMIXEL{};
		VoiceVec vVMIXER{};

		void Reset()
		{
			for (int i = 0; i < NUM_VOICES; i++)
			{
				decoder[i].Reset();
				adsr[i].Reset();
				volume[i].Reset();
			}
			KeyOn.full = 0;
			KeyOff.full = 0;
			PitchMod.full = 0;
			ENDX.full = 0;
			NON.full = 0;
			vNAX.Reset();
			Pitch.Reset();
			Counter.Reset();
			SamplePos.Reset();
			OUTX.Reset();
			VOLL.Reset();
			VOLR.Reset();
			ENVX.Reset();
		}

		void ProcessKonKoff()
		{
			if (KeyOn.full != 0 || KeyOff.full != 0)
			{
				for (int i = 0; i < NUM_VOICES; i++)
				{
					if ((KeyOff.full & (1 << i)) != 0U)
					{
						adsr[i].Release();
						//Console.WriteLn("SPU[%d]:VOICE[%d] Key Off", SPU_ID, i);
					}
					if ((KeyOn.full & (1 << i)) != 0U)
					{
						vNAX.arr[i].full = decoder[i].m_SSA.full;
						vNAX.arr[i].full++;

						ENDX.full &= ~(1 << i);
						adsr[i].Attack();
						Counter.uarr[i] = 0;
						SamplePos.uarr[i] = 0;

						decoder[i].m_DecodeHist1 = 0;
						decoder[i].m_DecodeHist2 = 0;
						decoder[i].m_Buffer = {};
						decoder[i].m_Wpos = 0;
						decoder[i].m_CustomLoop = false;
						//Console.WriteLn("SPU[%d]:VOICE[%d] Key On, SSA %08x", SPU_ID, i, decoder[i].m_SSA.full);
					}
				}
			}

			KeyOff.full = 0;
			KeyOn.full = 0;
		}

		s16* GetSample(int n)
		{
			return &decoder[n].m_Buffer[SamplePos.uarr[n] & 0x1f];
		}

		u16 BufferedSize(int n)
		{
			return static_cast<u16>(decoder[n].m_Wpos - SamplePos.uarr[n]);
		}

		__fi void DecodeSamples()
		{
			for (int i = 0; i < NUM_VOICES; i++)
			{
				// The block header (and thus LSA) updates every spu cycle
				decoder[i].m_CurHeader.bits = RAM[(vNAX.arr[i].full & ~0x7) & 0xfffff];
				if (decoder[i].m_CurHeader.LoopStart && !decoder[i].m_CustomLoop)
					decoder[i].m_LSA.full = vNAX.arr[i].full & ~0x7;

				// This doesn't exactly match the real behaviour,
				// it seems to initially decode a bigger chunk
				// and then decode more data after a bit has drained
				if (BufferedSize(i) >= 16)
				{
					// sufficient data buffered
					continue;
				}

				// Skip decoding for stopped voices.
				// technically not safe i think, envx could be written
				// directly to make it audible again.
				// But it's too good a speed boost to let go.
				if (adsr[i].GetPhase() != ADSR::Phase::Stopped)
				{
					u32 data = RAM[vNAX.arr[i].full & 0xfffff];
					for (int j = 0; j < 4; j++)
					{
						s32 sample = (s16)((data & 0xF) << 12);
						u8 filter = std::min<u8>(decoder[i].m_CurHeader.Filter, 4);

						// TODO do the right thing for invalid shift/filter values
						sample >>= decoder[i].m_CurHeader.Shift.GetValue();
						sample += (Decoder::adpcm_coefs_i[filter][0] * decoder[i].m_DecodeHist1) >> 6;
						sample += (Decoder::adpcm_coefs_i[filter][1] * decoder[i].m_DecodeHist2) >> 6;

						// We do get overflow here otherwise, should we?
						sample = std::clamp<s32>(sample, INT16_MIN, INT16_MAX);

						decoder[i].m_DecodeHist2 = decoder[i].m_DecodeHist1;
						decoder[i].m_DecodeHist1 = static_cast<s16>(sample);

						u16 pos = static_cast<u16>(decoder[i].m_Wpos + j) & 0x1f;
						decoder[i].m_Buffer[pos | 0x0] = sample;
						decoder[i].m_Buffer[pos | 0x20] = sample;

						data >>= 4;
					}
				}

				decoder[i].m_Wpos += 4;

				vNAX.arr[i].full++;

				if ((vNAX.arr[i].full & 0x7) == 0)
				{
					if (decoder[i].m_CurHeader.LoopEnd)
					{
						vNAX.arr[i].full = decoder[i].m_LSA.full;
						ENDX.full |= (1 << i);

						if (!decoder[i].m_CurHeader.LoopRepeat)
						{
							// Need to inhibit stopping here in noise is on
							// seems to result in the right thing but would like to verify
							if ((NON.full & (1 << i)) == 0U)
								adsr[i].Stop();
						}
					}

					vNAX.arr[i].full++;
				}
			}
		};


		__fi void UpdateVolume()
		{
			for (int i = 0; i < NUM_VOICES; i++)
			{
				ENVX.arr[i] = adsr[i].Level();
				VOLL.arr[i] = volume[i].left.GetCurrent();
				VOLR.arr[i] = volume[i].right.GetCurrent();

				adsr[i].Run();
				volume[i].Run();
			}
		};


		__fi bool IRQTest(u32 IRQA)
		{
			GSVector8i mask(GSVector8i(~0x7).broadcast32());
			GSVector8i irqa{GSVector8i((int)IRQA).broadcast32()};
			auto res = vNAX.vec[0] == irqa;
			res |= vNAX.vec[1] == irqa;
			res |= vNAX.vec[2] == irqa;

			res |= (vNAX.vec[0] & mask) == irqa;
			res |= (vNAX.vec[1] & mask) == irqa;
			res |= (vNAX.vec[2] & mask) == irqa;

			if (!res.allfalse())
			{
				return true;
			}

			return false;
		}

		__fi void Interpolate(VoiceVec& interp_out)
		{
			for (int i = 0; i < NUM_VOICES; i += 4)
			{
				GSVector8i dec(
					*(u64*)GetSample(i + 0),
					*(u64*)GetSample(i + 1),
					*(u64*)GetSample(i + 2),
					*(u64*)GetSample(i + 3));
				GSVector8i interp(
					*(u64*)gaussianTable[(Counter.uarr[i + 0] & 0xff0) >> 4].data(),
					*(u64*)gaussianTable[(Counter.uarr[i + 1] & 0xff0) >> 4].data(),
					*(u64*)gaussianTable[(Counter.uarr[i + 2] & 0xff0) >> 4].data(),
					*(u64*)gaussianTable[(Counter.uarr[i + 3] & 0xff0) >> 4].data());

				dec = dec.mul16hrs(interp);
				dec = dec.adds16(dec.yyww());
				auto lo = dec.adds16(dec.yxxxl());
				auto hi = dec.adds16(dec.yxxxh());

				interp_out.arr[i + 0] = lo.I16[0];
				interp_out.arr[i + 1] = hi.I16[4];
				interp_out.arr[i + 2] = lo.I16[8];
				interp_out.arr[i + 3] = hi.I16[12];
			}
		}

		__fi void UpdatePitch()
		{
			for (int i = 0; i < 2; i++)
			{
				GSVector8i steps{Pitch.vec[i].andnot(vPMON.vec[i])};
				GSVector8i factors{OUTX.vec[i]};
				auto pitch_lo = GSVector8i::i16to32(GSVector4i::cast(Pitch.vec[i]));
				auto factor_lo = GSVector8i::u16to32(GSVector4i::cast(factors));
				auto pitch_hi = GSVector8i::i16to32(GSVector4i::cast(Pitch.vec[i].cddd()));
				auto factor_hi = GSVector8i::u16to32(GSVector4i::cast(factors.cddd()));

				GSVector8i pmod_mask(GSVector8i(0xffff).broadcast32());
				auto reslo = pitch_lo.mul32lo(factor_lo).sra32(15) & pmod_mask;
				auto reshi = pitch_hi.mul32lo(factor_hi).sra32(15) & pmod_mask;
				steps |= reslo.pu32(reshi).acbd() & vPMON.vec[i];

				GSVector8i step_limit(GSVector8i(0x3fff).broadcast16());
				steps = Counter.vec[i].add16(steps.min_u16(step_limit));

				GSVector8i counter_mask(GSVector8i(0xfff).broadcast16());
				Counter.vec[i] = steps & counter_mask;
				SamplePos.vec[i] = SamplePos.vec[i].add16(steps.srl16(12));
			}
		}

		__fi void Mix(s16 cur_noise, VoiceVec& interp, AudioSample& dry, AudioSample& wet)
		{
			// Load noise into all elements
			GSVector8i noise[2]{
				GSVector8i::load(cur_noise).broadcast16(),
				GSVector8i::load(cur_noise).broadcast16()};

			// & with NON to only keep it for voices with noise selected
			noise[0] = noise[0] & vNON.vec[0];
			noise[1] = noise[1] & vNON.vec[1];

			// Load voice output, exclude voices with active noise
			GSVector8i samples[2]{
				interp.vec[0].andnot(vNON.vec[0]),
				interp.vec[1].andnot(vNON.vec[1])};

			// mix in noise
			samples[0] |= noise[0];
			samples[1] |= noise[1];

			// Apply ADSR volume
			samples[0] = samples[0].mul16hrs(ENVX.vec[0]);
			samples[1] = samples[1].mul16hrs(ENVX.vec[1]);

			// Save to OUTX (needs to be kept for later due to pitch mod)
			// offset it so it lines up with the voice using the data
			GSVector8i::store<false>(&OUTX.arr[1], samples[0].add16(GSVector8i(0x8000).broadcast16()));
			GSVector8i::store<false>(&OUTX.arr[17], samples[1].add16(GSVector8i(0x8000).broadcast16()));

			//MemOut(OutBuf::Voice1, samples[0].I16[1]);
			//MemOut(OutBuf::Voice3, samples[0].I16[3]);

			// Split to streo and apply l/r volume
			GSVector8i left[2], right[2];
			left[0] = samples[0].mul16hrs(VOLL.vec[0]);
			left[1] = samples[1].mul16hrs(VOLL.vec[1]);
			right[0] = samples[0].mul16hrs(VOLR.vec[0]);
			right[1] = samples[1].mul16hrs(VOLR.vec[1]);

			GSVector8i vc_dry_l[2], vc_dry_r[2], vc_wet_l[2], vc_wet_r[2];
			vc_dry_l[0] = left[0] & vVMIXL.vec[0];
			vc_dry_l[1] = left[1] & vVMIXL.vec[1];
			vc_dry_r[0] = right[0] & vVMIXR.vec[0];
			vc_dry_r[1] = right[1] & vVMIXR.vec[1];

			vc_wet_l[0] = left[0] & vVMIXEL.vec[0];
			vc_wet_l[1] = left[1] & vVMIXEL.vec[1];
			vc_wet_r[0] = right[0] & vVMIXER.vec[0];
			vc_wet_r[1] = right[1] & vVMIXER.vec[1];

			dry = {hsum(vc_dry_l[0], vc_dry_l[1]), hsum(vc_dry_r[0], vc_dry_r[1])};
			wet = {hsum(vc_wet_l[0], vc_wet_l[1]), hsum(vc_wet_r[0], vc_wet_r[1])};
		}

		__fi void Run(AudioSample& dry_out, AudioSample& wet_out)
		{

			ProcessKonKoff();
			DecodeSamples();
			VoiceVec interp_out{};
			Interpolate(interp_out);
			UpdateVolume();
			UpdatePitch();
			//m_voice.Mix(m_Noise.Get(), interp_out, VoicesDry, VoicesWet);
			Mix(0, interp_out, dry_out, wet_out);
		}

		[[nodiscard]] u16 Read(u32 id, u32 addr) const
		{
			switch (addr)
			{
				case 0:
					return volume[id].left.Get();
				case 2:
					return volume[id].right.Get();
				case 4:
					return Pitch.uarr[id];
				case 6:
					return adsr[id].m_Reg.lo.GetValue();
				case 8:
					return adsr[id].m_Reg.hi.GetValue();
				case 10:
					return adsr[id].Level();
				case 12:
					return volume[id].left.GetCurrent();
				case 14:
					return volume[id].right.GetCurrent();
				default:
					pxAssertMsg(false, "Unhandled SPU Write");
					return 0;
			}
		}

		void Write(u32 id, u32 addr, u16 value)
		{
			switch (addr)
			{
				case 0:
					volume[id].left.Set(value);
					return;
				case 2:
					volume[id].right.Set(value);
					return;
				case 4:
					Pitch.uarr[id] = value;
					return;
				case 6:
					adsr[id].m_Reg.lo.SetValue(value);
					adsr[id].UpdateSettings();
					return;
				case 8:
					adsr[id].m_Reg.hi.SetValue(value);
					adsr[id].UpdateSettings();
					return;
				case 10:
					// writeable envx
					adsr[id].SetLevel(static_cast<s16>(value));
					return;
				case 12:
				case 14:
					// These two are not writeable though
					return;
				default:
					pxAssertMsg(false, "Unhandled SPU Write");
			}
		}

		// The new (for SPU2) full addr regs are a separate range
		[[nodiscard]] u16 ReadAddr(u32 id, u32 addr) const
		{
			switch (addr)
			{
				case 0:
					return decoder[id].m_SSA.hi.GetValue();
				case 2:
					return decoder[id].m_SSA.lo.GetValue();
				case 4:
					return decoder[id].m_LSA.hi.GetValue();
				case 6:
					return decoder[id].m_LSA.lo.GetValue();
				case 8:
					return vNAX.arr[id].hi.GetValue();
				case 10:
					return vNAX.arr[id].lo.GetValue();
				default:
					pxAssertMsg(false, "Unhandled SPU Write");
					return 0;
			}
		}

		void WriteAddr(u32 id, u32 addr, u16 value)
		{
			switch (addr)
			{
				case 0:
					decoder[id].m_SSA.hi.SetValue(value & 0xF);
					return;
				case 2:
					decoder[id].m_SSA.lo.SetValue(value);
					return;
				case 4:
					decoder[id].m_LSA.hi.SetValue(value & 0xF);
					decoder[id].m_CustomLoop = true;
					return;
				case 6:
					decoder[id].m_LSA.lo.SetValue(value);
					decoder[id].m_CustomLoop = true;
					return;
				case 8:
					vNAX.arr[id].hi.SetValue(value & 0xF);
					return;
				case 10:
					vNAX.arr[id].lo.SetValue(value);
					return;
				default:
					pxAssertMsg(false, "Unhandled SPU Write");
			}
		}
	};

} // namespace SPU
