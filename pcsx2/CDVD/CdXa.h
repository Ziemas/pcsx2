#pragma once
#include "IopCommon.h"
#include "PrecompiledHeader.h"

struct xa_subheader
{
	u8 filenum;
	u8 channum;
	u8 submode;
	u8 coding;

	u8 filenum2;
	u8 channum2;
	u8 submode2;
	u8 coding2;
};


struct ADPCM_Decode
{
	s32 y0, y1;
};

struct xa_decode
{
	// Sample rate which can be anywhere from 44100 to 37800 to 18900
	s32 freq;
	s32 nbits;
	s32 stereo;
	s32 nsamples;
	ADPCM_Decode left, right;
	s16 pcm[16384];
};

void DecodeADPCM(xa_decode& decoded, u8* xaData, s32 last_l, s32 last_r);
void DecodeChunck(ADPCM_Decode* decp, u8 filter, u8* blk, s16* samples, s32 last_sampleL, s32 last_sampleR);
