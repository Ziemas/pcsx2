#include "PrecompiledHeader.h"
#include "Global.h"
#include "SndOut.h"
#include "cubeb/cubeb.h"

class CubebOutput : public SndOutModule
{
public:
	s32 Init()
	{
		ConLog("Starting Cubeb output\n");
		cubeb_init(&m_ctx, "pcsx2", NULL);

		cubeb_stream_params params = {};
		params.format = CUBEB_SAMPLE_S16LE;
		params.channels = 2; // TODO: surround stuff
		params.layout = CUBEB_LAYOUT_STEREO;
		params.rate = 48000;

		int rv = cubeb_get_min_latency(m_ctx, &params, &m_latency);
		if (rv != CUBEB_OK)
		{
			ConLog("Cubeb: could not get minimum latency\n");
			return -1;
		}

		rv = cubeb_stream_init(m_ctx, &m_strm, "pcsx2", NULL, NULL, NULL, &params, m_latency, data_cb, state_cb, NULL);
		if (rv != CUBEB_OK)
		{
			ConLog("Cubeb: Could not init stream\n");
			return -1;
		}

		rv = cubeb_stream_start(m_strm);
		if (rv != CUBEB_OK)
		{
			ConLog("Cubeb: Could not start stream\n");
			return -1;
		}

		return 0;
	}

	void Close()
	{
		cubeb_stream_stop(m_strm);
		cubeb_stream_destroy(m_strm);
		cubeb_destroy(m_ctx);
	}

	s32 Test() const { return 0; }
	void Configure(uptr parent) {}
	void ReadSettings() {}
	void SetApiSettings(wxString api) {}
	void WriteSettings() const {}
	int GetEmptySampleCount() { return 0; }

	const wchar_t* GetIdent() const { return L"cubeb"; }
	const wchar_t* GetLongName() const { return L"cubeb"; }

private:
	static long data_cb(cubeb_stream* stm, void* user,
						const void* input_buffer, void* output_buffer, long nframes)
	{
		StereoOut16* output = (StereoOut16*)output_buffer;

		SndBuffer::ReadSamples(output, nframes);

		return nframes;
	}

	static void state_cb(cubeb_stream* stm, void* user, cubeb_state state)
	{
		printf("state=%d\n", state);
	}

	cubeb* m_ctx = nullptr;
	cubeb_stream* m_strm = nullptr;
	u32 m_latency = 0;
};

static CubebOutput cubeb_out;

SndOutModule* CubebOut = &cubeb_out;
