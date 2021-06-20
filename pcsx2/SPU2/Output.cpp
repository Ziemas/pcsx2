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

#include "Output.h"
#include "common/Console.h"

namespace SPU
{
	void SndOutput::Init()
	{
		if (m_Init)
			return;

		Console.WriteLn("Starting sound output");

		cubeb_init(&m_Ctx, "PCSX2", nullptr);

		s32 err = 0;

		// Resample to preferred rate at some point?
		//u32 rate = 0;
		//err = cubeb_get_preferred_sample_rate(m_Ctx, &rate);
		//if (err != CUBEB_OK)
		//{
		//	Console.Error("Audio backend: Could not get preferred sample rate");
		//	return;
		//}

		cubeb_stream_params outparam = {};
		outparam.channels = 2;
		outparam.format = CUBEB_SAMPLE_S16LE;
		outparam.rate = 48000;
		outparam.layout = CUBEB_LAYOUT_STEREO;
		outparam.prefs = CUBEB_STREAM_PREF_NONE;

		u32 latency = 0;
		err = cubeb_get_min_latency(m_Ctx, &outparam, &latency);
		if (err != CUBEB_OK)
		{
			Console.Error("Audio backend: Could not get minimum latency");
			return;
		}

		err = cubeb_stream_init(m_Ctx, &m_Stream, "SPU Output", nullptr, nullptr, nullptr,
			&outparam, latency, &SoundCB, &StateCB, nullptr);
		if (err != CUBEB_OK)
		{
			Console.Error("Audio backend: Could not open stream");
			return;
		}

		cubeb_set_log_callback(CUBEB_LOG_VERBOSE, &LogCB);

		err = cubeb_stream_start(m_Stream);
		if (err != CUBEB_OK)
		{
			Console.Error("Audio backend: Could not start stream");
		}

		m_Init = true;
	}

	void SndOutput::Shutdown()
	{
		Console.WriteLn("Audio backend: Shutting down output stream");
		cubeb_stream_stop(m_Stream);
		cubeb_stream_destroy(m_Stream);
		cubeb_destroy(m_Ctx);
		m_Init = false;
	}

	void SndOutput::LogCB(char const* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);

		Console.FormatV(fmt, args);

		va_end(args);
	}

	long SndOutput::SoundCB(cubeb_stream* stream, void* user, const void* input_buffer,
		void* output_buffer, long nframes)
	{
		return 0;
	}

	void SndOutput::StateCB(cubeb_stream* stream, void* user, cubeb_state state)
	{
		switch (state)
		{
			case CUBEB_STATE_ERROR:
				Console.Error("Audio output stopped due to error");
				break;
			case CUBEB_STATE_DRAINED:
				Console.Warning("Audio stream drained");
				break;
			default:
				break;
		}
	}
} // namespace SPU
