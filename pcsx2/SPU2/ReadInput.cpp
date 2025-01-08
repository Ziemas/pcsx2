// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Dma.h"
#include "IopDma.h"
#include "IopHw.h"
#include "SPU2/Debug.h"
#include "SPU2/defs.h"
#include "SPU2/spu2.h"

/*
** ADMA/Block Transfers lets games stream raw PCM to the SPU and have it be mixed into the output.
**
** There are also an additional mode for streaming a raw bitstream directly to the SPDIF output
** using core0's ADMA buffer.
**
** Core1 seems to have a similar thing for streaming 32bit PCM which is only(?) used by the BIOS
** cd player. This functionality is not available to normal games.
*/

void V_Core::RunAdma()
{
	if (OutPos == 0x100 || OutPos == 0x0)
	{
		if (OutPos == 0x100)
			InputPosWrite = 0;
		else if (OutPos == 0)
			InputPosWrite = 0x100;

		bool adma_enable = ((AutoDMACtrl & (Index + 1)) == (Index + 1));
		// Signal DMA that we're ready to recieve.
		if (adma_enable)
		{
			DmaReq = true;
		}
	}
}

StereoOut32 V_Core::ReadInput()
{
	StereoOut32 retval;

	for (int i = 0; i < 2; i++)
		if (Cores[i].IRQEnable && (0x2000 + (Index << 10) + OutPos) == (Cores[i].IRQA & 0xfffffdff))
			SetIrqCall(i);

	retval = StereoOut32(
		(s32)(*GetMemPtr(0x2000 + (Index << 10) + OutPos)),
		(s32)(*GetMemPtr(0x2200 + (Index << 10) + OutPos)));

#ifdef PCSX2_DEVBUILD
	DebugCores[Index].admaWaveformL[OutPos % 0x100] = retval.Left;
	DebugCores[Index].admaWaveformR[OutPos % 0x100] = retval.Right;
#endif

	return retval;
}
