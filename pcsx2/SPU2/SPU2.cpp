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

#include "SPU2.h"
#include "SpuCore.h"
#include "common/Console.h"

namespace SPU
{
	u16 SPU_RAM[1024 * 1024 * 2] = {};
	//std::array<u16, 1024*1024*2> SPU_RAM = {};
	SPUCore cores[2] = {
		{*SPU_RAM, 0},
		{*SPU_RAM, 1},
	};

	void Run(u32 cycles)
	{
		//Console.WriteLn("SPU run %d cycles", cycles);
	}
	void InterruptDMA4()
	{
		Console.WriteLn("SPU irq dma4");
	}
	void InterruptDMA7()
	{
		Console.WriteLn("SPU irq dma7");
	}
	void WriteDMA7Mem(u16* madr, u32 size)
	{
		Console.WriteLn("SPU DMA7 WRITE");
	}
	void ReadDMA7Mem(u16* madr, u32 size)
	{
		Console.WriteLn("SPU DMA7 READ");
	}
	void WriteDMA4Mem(u16* madr, u32 size)
	{
		Console.WriteLn("SPU DMA4 WRITE");
	}
	void ReadDMA4Mem(u16* madr, u32 size)
	{
		Console.WriteLn("SPU DMA4 READ");
	}
	u16 Read(u32 addr)
	{
		u32 core = (addr >> 10) & 1;

		addr &= 0x7FF;
		if (addr > 0x760) // SPDIF regs range
			return 0;

		addr &= 0x3FF;

		Console.WriteLn("SPU %d READ %04x", core, addr);
		return 0;
	}
	void Write(u32 addr, u16 value)
	{
		u32 core = (addr >> 10) & 1;

		addr &= 0x7FF;
		if (addr > 0x760) // SPDIF regs range
			return;

		addr &= 0x3FF;

		Console.WriteLn("SPU %d write %04x -> %04x", core, value, addr);
	}
	void Reset(PS2Modes isRunningPSXMode)
	{
		Console.WriteLn("SPU RESET");
	}
	bool SetupRecording(std::string* filename)
	{
		return false;
	}
	bool EndRecording()
	{
		return false;
	}
	void Configure() {}
	void Close() {}
	void Shutdown() {}

	s32 Open(PS2Modes mode)
	{
		return 0;
	}

	s32 Init()
	{
		return 0;
	}

	s32 Freeze(FreezeAction mode, freezeData* data)
	{
		return 0;
	}

	bool IsRunningPSXMode() { return false; };

	void SetOutputPaused(bool paused) {}

} // namespace SPU
