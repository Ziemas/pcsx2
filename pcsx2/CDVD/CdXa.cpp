#include "PrecompiledHeader.h"
#include "SPU2/Global.h"
#include "SPU2/Mixer.h"
#include "IopCommon.h"
#include "CdXa.h"

#include <algorithm>
#include <array>

// No$ calls this pos_xa_adpcm_table. This is also the other neg one as well
static const s8 tbl_XA_Factor[16][2] = {
	{0, 0},
	{60, 0},
	{115, -52},
	{98, -55},
	{122, -60}};

void DecodeChunck(ADPCM_Decode* decp, u8 filter, u8* blk, u16* samples, s32 last_sampleL, s32 last_sampleR)
{
	for (u32 block = 0; block < cdr.Xa.nbits; block++)
	{
		const u16 header = filter;
		const u8 filterID = header >> 4 & 0xF;

		u8 filterPos = filter;
		u8 filterNeg = tbl_XA_Factor[filterID][1];

		u8* wordP = blk + 16; // After 16 byts or the "28 Word Data Bytes"

		for (int i = 0; i < 28; i++)
		{

			u32 word_data; // The Data words are 32bit little endian
			std::memcpy(&word_data, &wordP[i * sizeof(u32)], sizeof(word_data));

			u16 nibble = nibble = ((word_data >> 4) & 0xF); // I'm looking for the octect value of the binary
			const u8 shift = 12 - (blk[4 + block * 2 + nibble]);

			s16 data = (*blk << 12) >> shift; // shift left 12 shift right by calculated value to get 16 bit sample data out. nibble messes the equation up right now.
			s32 pcm = data + (((filterPos * last_sampleL) + (filterNeg * last_sampleR) + 32) >> 6);

			Clampify(pcm, -0x8000, 0x7fff);
			*(samples++) = pcm;

			s32 pcm2 = data + (((filterPos * pcm) + (filterNeg * last_sampleL) + 32) >> 6);

			Clampify(pcm2, -0x8000, 0x7fff);
			*(samples++) = pcm2;

			decp->y0 = pcm;
			decp->y1 = pcm2;

			std::cout << "PCM1: " << pcm << std::endl;
			std::cout << "PCM2: " << pcm2 << std::endl;
		}
	}
}

void DecodeADPCM(xa_decode& decoded, u8* xaData, s32 last_left, s32 last_right)
{
	/*************************************************************************
	* Taken from No$
	*  src=src+12+4+8   ;skip sync,header,subheader
	*  for i=0 to 11h
	*   for blk=0 to 3
	*    IF stereo ;
	*       left-samples (LO-nibbles), plus right-samples (HI-nibbles)
	*      decode_28_nibbles(src,blk,0,dst_left,old_left,older_left)
	*      decode_28_nibbles(src,blk,1,dst_right,old_right,older_right)
	*    ELSE      ;first 28 samples (LO-nibbles), plus next 28 samples (HI-nibbles)
	*      decode_28_nibbles(src,blk,0,dst_mono,old_mono,older_mono)
	*      decode_28_nibbles(src,blk,1,dst_mono,old_mono,older_mono)
	*    ENDIF
	*   next blk
	*   src=src+128
	*  next i
	*  src=src+14h+4    ;skip padding,edc
	*******************************************************************************************/

	const u8* sound_groupsp;
	const u8 *sound_datap, *sound_datap2;
	int i, j, k, nbits;
	u16 data[4096];

	nbits = decoded.nbits == 4 ? 4 : 2;
	// 16 byte header. Shift, Filter
	sound_groupsp = xaData + 4;
	// 16 bytes after header
	sound_datap = sound_groupsp + 16;

	Console.Warning("DECODE ADPCM");

	for (u32 block = 0; block < 18; block++)
	{
		if (decoded.stereo)
		{
			DecodeChunck(&decoded.left, sound_groupsp[tbl_XA_Factor[block][0]], xaData, data, last_left, last_right); // Note we access the positive table first
			DecodeChunck(&decoded.right, sound_groupsp[tbl_XA_Factor[block][0]], xaData, data, last_left, last_right);
		}
		else
		{
			// Mono sound
			DecodeChunck(&decoded.left, sound_groupsp[tbl_XA_Factor[block][0]], xaData, data, last_left, last_right); // Note we access the positive table first
		}

		std::cout << "Data: " << unsigned(data[block]) << std::endl;
	}
}
