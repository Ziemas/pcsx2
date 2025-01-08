// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "PrecompiledHeader.h"
#include "common/Console.h"
#include "SPU2/defs.h"
#include "SPU2/Debug.h"
#include "IopDma.h"
#include "SPU2/Dma.h"
#include "SPU2/spu2.h"
#include "R3000A.h"
#include "IopHw.h"
#include "Config.h"

#ifdef PCSX2_DEVBUILD

#define safe_fclose(ptr) \
	((void)((((ptr) != nullptr) && (std::fclose(ptr), !!0)), (ptr) = nullptr))

static FILE* DMA4LogFile = nullptr;
static FILE* DMA7LogFile = nullptr;
static FILE* ADMA4LogFile = nullptr;
static FILE* ADMA7LogFile = nullptr;
static FILE* ADMAOutLogFile = nullptr;

static FILE* REGWRTLogFile[2] = {0, 0};

void DMALogOpen()
{
	if (!SPU2::DMALog())
		return;
	DMA4LogFile = EmuFolders::OpenLogFile("SPU2dma4.dat", "wb");
	DMA7LogFile = EmuFolders::OpenLogFile("SPU2dma7.dat", "wb");
	ADMA4LogFile = EmuFolders::OpenLogFile("adma4.raw", "wb");
	ADMA7LogFile = EmuFolders::OpenLogFile("adma7.raw", "wb");
	ADMAOutLogFile = EmuFolders::OpenLogFile("admaOut.raw", "wb");
}

void DMA4LogWrite(void* lpData, u32 ulSize)
{
	if (!SPU2::DMALog())
		return;
	if (!DMA4LogFile)
		return;
	fwrite(lpData, ulSize, 1, DMA4LogFile);
}

void DMA7LogWrite(void* lpData, u32 ulSize)
{
	if (!SPU2::DMALog())
		return;
	if (!DMA7LogFile)
		return;
	fwrite(lpData, ulSize, 1, DMA7LogFile);
}

void RegWriteLog(u32 core, u16 value)
{
	if (!SPU2::DMALog())
		return;
	if (!REGWRTLogFile[core])
		return;
	fwrite(&value, 2, 1, REGWRTLogFile[core]);
}

void DMALogClose()
{
	safe_fclose(DMA4LogFile);
	safe_fclose(DMA7LogFile);
	safe_fclose(REGWRTLogFile[0]);
	safe_fclose(REGWRTLogFile[1]);
	safe_fclose(ADMA4LogFile);
	safe_fclose(ADMA7LogFile);
	safe_fclose(ADMAOutLogFile);
}

#endif

void V_Core::LogAutoDMA(FILE* fp)
{
	if (!SPU2::DMALog() || !fp)
		return;
	// TODO Fix no more dmaptr
	//fwrite(DMAPtr + InputDataProgress, 0x400, 1, fp);
}

static void updateCycleTarget(s32 target)
{
	if (((psxCounters[6].startCycle + psxCounters[6].deltaCycles) - psxRegs.cycle) > (u32)target)
	{
		psxCounters[6].startCycle = psxRegs.cycle;
		psxCounters[6].deltaCycles = target;

		psxNextDeltaCounter -= (psxRegs.cycle - psxNextStartCounter);
		psxNextStartCounter = psxRegs.cycle;
		if (psxCounters[6].deltaCycles < psxNextDeltaCounter)
			psxNextDeltaCounter = psxCounters[6].deltaCycles;
	}
}

void V_Core::SetDmaIrq()
{
	if (Index == 0)
	{
		SPU2::FileLog("[%10d] SPU2 interruptDMA4\n", Cycles);
		spu2DMA4Irq();
	}
	else
	{
		SPU2::FileLog("[%10d] SPU2 interruptDMA7\n", Cycles);
		spu2DMA7Irq();
	}
}

void V_Core::StartDma(DmaDirection dir, u32 addr, u32 size)
{
	DmaDir = dir;
	DmaSize = size;
	MADR = addr;

	// TODO figure these out?
	Regs.STATX &= ~0x80;
	Regs.STATX |= 0x400;

	if (dir == DmaDirection::Read && SPU2::MsgDMA())
	{
		SPU2::ConLog("* SPU2: DMA%c Read Transfer of %d bytes from %x (%02x %x %04x). IRQE = %d IRQA = %x MADR = %x \n",
			GetDmaIndexChar(), size << 1, ActiveTSA, DMABits, AutoDMACtrl, Regs.ATTR & 0xffff,
			Cores[Index].IRQEnable, Cores[Index].IRQA, MADR);
	}

	if (dir == DmaDirection::Write && SPU2::MsgDMA())
	{
		SPU2::ConLog("* SPU2: DMA%c Write Transfer of %d bytes to %x (%02x %x %04x). IRQE = %d IRQA = %x  MADR = %x\n",
			GetDmaIndexChar(), size << 1, ActiveTSA, DMABits, AutoDMACtrl, Regs.ATTR & 0xffff,
			Cores[Index].IRQEnable, Cores[Index].IRQA, MADR);
	}

	DmaCounter = 0;
	LastClock = psxRegs.cycle;
	updateCycleTarget(DmaCounter);
	DmaReq = true;

	RunDma();
}

static void invalidateCache(u32 start, u32 end)
{
	const u32 cacheIdxStart = start / pcm_WordsPerBlock;
	const u32 cacheIdxEnd = (end + pcm_WordsPerBlock - 1) / pcm_WordsPerBlock;
	PcmCacheEntry* cacheLine = &pcm_cache_data[cacheIdxStart];
	PcmCacheEntry& cacheEnd = pcm_cache_data[cacheIdxEnd];

	cacheLine->Validated = false;
	while (cacheLine != &cacheEnd)
	{
		cacheLine++;
		cacheLine->Validated = false;
	}
}

static void dmaTestIrq(u32 start, u32 end)
{
	for (int i = 0; i < 2; i++)
	{
		if (Cores[i].IRQEnable && (Cores[i].IRQA >= start && Cores[i].IRQA < end))
		{
			Console.WriteLn("[%d] DMA HIT IRQA %x", i, Cores[i].IRQA);
			SetIrqCallDma(i);
		}
	}
}

// Transfering in FIFO sized chunks, accurate but maybe a bit ridiculus...
static constexpr int dma_fifo_size = 32;
static constexpr int cycles_per_fifo = dma_fifo_size * 48;

void V_Core::RunDma()
{
	if (!DmaReq)
		return;

	// TODO remove or keep?
	if (DmaSize == 0)
	{
		Regs.STATX |= 0x80;

		SPU2::ConLog("* SPU2: DMA%c Transfer Complete. TSA = %x IRQE = %d IRQA = %x \n",
			GetDmaIndexChar(), ActiveTSA, Cores[Index].IRQEnable, Cores[Index].IRQA);

		DmaReq = false;
		return;
	}

	// Check timer counting down to next transfer portion
	u32 lapsed = psxRegs.cycle - LastClock;
	DmaCounter -= lapsed;
	//Console.WriteLn(ConsoleColors::Color_Red, "[%d] counter %d lapsed %d", Index, DmaCounter, lapsed);
	LastClock = psxRegs.cycle;
	if (DmaCounter > 0)
	{
		updateCycleTarget(DmaCounter);
		return;
	}

	//Console.WriteLn(ConsoleColors::Color_Yellow, "[%d] performing transfer part %x", Index, ActiveTSA);
	bool adma_enable = ((AutoDMACtrl & (Index + 1)) == (Index + 1));
	s32 transfer_size = dma_fifo_size;
	s32 transfer_size_bytes = transfer_size * 2;
	u8* mem = iopPhysMem(MADR);

	// TODO we want the spu irq for the last fifo transfer come after the dma irq
	// since the spu is still writing out the fifo after dma completion.
	//
	// (althugh it may not matter since libsd waits for the fifo to drain before
	// calling the users transfer handler)
	dmaTestIrq(ActiveTSA, ActiveTSA + transfer_size);

	if (DmaDir == DmaDirection::Write)
	{
		invalidateCache(ActiveTSA, ActiveTSA + transfer_size);
		memcpy(GetMemPtr(ActiveTSA), mem, transfer_size_bytes);
	}
	else if (DmaDir == DmaDirection::Read)
	{
		// Invalidate IOP Memory
		psxCpu->Clear(MADR, transfer_size_bytes);
		memcpy(mem, GetMemPtr(ActiveTSA), transfer_size_bytes);
	}

	ActiveTSA = (ActiveTSA + transfer_size) & 0xf'ffff;
	DmaSize = std::max<s32>(0, DmaSize - transfer_size);
	MADR += transfer_size_bytes;

	if (DmaSize == 0)
	{
		// Fire IRQ but don't stop yet, run the counter again
		// to simulate the FIFO draining

		SetDmaIrq();
	}

	DmaCounter = cycles_per_fifo;
	updateCycleTarget(DmaCounter);
}
