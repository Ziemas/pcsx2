// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#define _PC_ // disables MIPS opcode macros.

#include "R3000A.h"
#include "Common.h"
#include "Sif.h"
#include "IopHw.h"

_sif sif2;

#define SIF_LOG Console.WriteLn
#define DMA_LOG Console.WriteLn

static void Sif2Init()
{
	SIF_LOG("SIF2 DMA start... free %x iop busy %x", sif2.fifo.sif_free(), sif2.iop.busy);
}

bool WriteFifoSingleWord()
{
	// There's some data ready to transfer into the fifo..

	SIF_LOG("Write Single word to SIF2 Fifo");

	sif2.fifo.write((u32*)&psxHu32(HW_PS1_GPU_DATA), 1);
	if (sif2.fifo.size > 0)
		psxHu32(0x1000f300) &= ~0x4000000;
	return true;
}

bool ReadFifoSingleWord()
{
	u32 ptag[4];

	//SIF_LOG(" EE SIF doing transfer %04Xqw to %08X", readSize, sif2dma.madr);
	SIF_LOG("Read Fifo SIF2 Single Word IOP Busy %x Fifo Size %x SIF2 CHCR %x", sif2.iop.busy, sif2.fifo.size, HW_DMA2_CHCR);


	sif2.fifo.read((u32*)&ptag[0], 1);
	psHu32(0x1000f3e0) = ptag[0];
	if (sif2.fifo.size == 0)
		psxHu32(0x1000f300) |= 0x4000000;
	if (sif2.iop.busy && sif2.fifo.size <= 8)
		SIF2Dma();
	return true;
}

static bool WriteEEtoFifo()
{
	if (sif2.fifo.sif_free() <= 0)
		return 0;

	const int writeSize = std::min((s32)sif2dma.qwc, sif2.fifo.sif_free() >> 2);

	SIF_LOG("Write EE to FIFO: +++++++++++ %lX of %lX", writeSize << 2, sif2dma.qwc << 2);

	void* addr = sif2dma.getAddr(sif2dma.madr, DMAC_SIF2, false);
	if (addr == NULL)
	{
		DevCon.Warning("Write EE to Fifo: addr == NULL");
		return false;
	}

	sif2.fifo.write((u32*)addr, writeSize << 2);

	sif2dma.madr += writeSize << 4;
	sif2.ee.cycles += writeSize;
	sif2dma.qwc -= writeSize;

	if (sif2dma.qwc <= 0)
	{
		sif2.ee.end = true;
	}

	return true;
}


// Write from Fifo to EE.
static bool WriteFifoToEE()
{
	if (sif2.fifo.size <= 0)
		return 0;

	const int readSize = std::min((s32)sif2dma.qwc, sif2.fifo.size >> 2);

	SIF_LOG("Write Fifo to EE: ----------- %lX of %lX", readSize << 2, sif2dma.qwc << 2);

	// Not actually a dma tag, but the address is fine
	void* addr = sif2dma.getAddr(sif2dma.madr, DMAC_SIF2, true);
	if (addr == NULL)
	{
		DevCon.Warning("Write Fifo to EE: addr == NULL");
		return false;
	}

	sif2.fifo.read((u32*)addr, readSize << 2);

	sif2dma.madr += readSize << 4;
	sif2.ee.cycles += readSize;
	sif2dma.qwc -= readSize;

	if (sif2dma.qwc <= 0)
	{
		sif2.ee.end = true;
	}

	return true;
}

// Write IOP to Fifo.
static bool WriteIOPtoFifo()
{
	if (sif2.fifo.sif_free() <= 0)
		return 0;

	// There's some data ready to transfer into the fifo..
	const int writeSize = std::min(sif2.iop.counter, sif2.fifo.sif_free());

	SIF_LOG("Write IOP to Fifo: +++++++++++ %lX of %lX", writeSize, sif2.iop.counter);

	sif2.fifo.write((u32*)iopPhysMem(hw_dma2.madr), writeSize);
	hw_dma2.madr += writeSize << 2;

	sif2.iop.cycles += writeSize;
	sif2.iop.counter -= writeSize;

	if (sif2.iop.counter <= 0)
	{
		if (sif2.fifo.size & 3)
		{
			sif2.fifo.writeJunk(4 - (sif2.fifo.size & 3));
		}

		sif2.iop.end = true;
	}

	return true;
}

static bool WriteFifoToIOP()
{
	if (sif2.fifo.size < 4)
		return 0;

	const int readSize = std::min(sif2.iop.counter, sif2.fifo.size);

	SIF_LOG("Write Fifo to IOP: ----------- %lX of %lX", readSize, sif2.iop.counter);

	sif2.fifo.read((u32*)iopPhysMem(hw_dma2.madr), readSize);

	psxCpu->Clear(hw_dma2.madr, readSize);
	hw_dma2.madr += readSize << 2;

	sif2.iop.cycles += readSize >> 2;
	sif2.iop.counter -= readSize;

	if (sif2dma.qwc <= 0 && sif2.fifo.size == 0)
	{
		sif2.iop.counter = 0;
	}

	if (sif2.iop.counter <= 0)
	{
		sif2.iop.end = true;
	}

	return true;
}

// Stop transferring ee, and signal an interrupt.
static void EndEE()
{
	SIF_LOG("Sif2: End EE");
	sif2.ee.end = false;
	sif2.ee.busy = false;
	if (sif2.ee.cycles == 0)
	{
		SIF_LOG("SIF2 EE: cycles = 0");
		sif2.ee.cycles = 1;
	}

	//CPU_INT(DMAC_SIF2, sif2.ee.cycles * BIAS);
	CPU_INT(DMAC_SIF2, 0);
}

// Stop transferring iop, and signal an interrupt.
static void EndIOP()
{
	SIF_LOG("Sif2: End IOP");
	sif2data = 0;
	sif2.iop.end = false;
	sif2.iop.busy = false;

	if (sif2.iop.cycles == 0)
	{
		DevCon.Warning("SIF2 IOP: cycles = 0");
		sif2.iop.cycles = 1;
	}

	//PSX_INT(IopEvt_SIF2, sif2.iop.cycles);
	PSX_INT(IopEvt_SIF2, 0);
}

// Handle the EE transfer.
static int HandleEETransfer()
{
	if (sif2dma.qwc <= 0)
		return false;

	if (sif2dma.chcr.DIR)
	{
		return WriteEEtoFifo();
	}
	else
	{
		return WriteFifoToEE();
	}
}

static int HandleIOPTransfer()
{
	if (sif2.iop.counter <= 0)
		return 0;

	if (HW_DMA2_CHCR & 1)
	{
		return WriteIOPtoFifo();
	}
	else
	{
		return WriteFifoToIOP();
	}
}

static void Sif2End()
{
	if (sif2.ee.end && sif2.iop.end)
	{
		psHu32(SBUS_F240) &= ~0x80;
		psHu32(SBUS_F240) &= ~0x8000;

		EndIOP();
		EndEE();

		DMA_LOG("SIF2 DMA End");

		sif2.ee.cycles = 0;
		sif2.iop.cycles = 0;
	}
}

void SIF2Dma()
{
	int BusyCheck = 0;
	Sif2Init();

	do
	{
		BusyCheck = 0;

		if (sif2.iop.busy)
		{
			BusyCheck += HandleIOPTransfer();
		}

		if (sif2.ee.busy)
		{
			BusyCheck += HandleEETransfer();
		}

	} while (BusyCheck > 0);

	Sif2End();
}

void sif2Interrupt()
{
	SIF_LOG("SIF2 IOP Intr end");
	HW_DMA2_CHCR &= ~0x01000000;
	psxDmaInterrupt(2);
}

void EEsif2Interrupt()
{
	SIF_LOG("SIF2 EE Intr end");
	hwDmacIrq(DMAC_SIF2);
	sif2dma.chcr.STR = false;
}

void dmaSIF2()
{
	DevCon.Warning("SIF2 EE CHCR %x", sif2dma.chcr._u32);
	SIF_LOG("dmaSIF2%s", sif2dma.cmqt_to_str().c_str());

	sif2.fifo.clear();
	if (sif2.fifo.readPos != sif2.fifo.writePos)
	{
		SIF_LOG("warning, sif2.fifoReadPos != sif2.fifoWritePos");
	}

	psHu32(SBUS_F240) |= 0x8000;
	sif2.ee.busy = true;

	SIF2Dma();
}
