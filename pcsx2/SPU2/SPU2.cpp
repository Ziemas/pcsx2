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

namespace SPU
{
	u16 SPU_RAM[1024 * 1024 * 2] = {};
	//std::array<u16, 1024*1024*2> SPU_RAM = {};
	SPUCore cores[2] = {
		{*SPU_RAM, 0},
		{*SPU_RAM, 1},
	};

	void SPUCore::Write16() {}
	u16 SPUCore::Read16(u32 addr) { return 0; }

	void Run(u32 cycles) {}
	void InterruptDMA4() {}
	void InterruptDMA7() {}
	void WriteDMA7Mem(u16* madr, u32 size) {}
	void ReadDMA7Mem(u16* madr, u32 size) {}
	void WriteDMA4Mem(u16* madr, u32 size) {}
	void ReadDMA4Mem(u16* madr, u32 size) {}
	u16 Read(u32 addr) { return 0; }
	void Write(u32 addr, u16 value) {}
	void Reset(PS2Modes isRunningPSXMode) {}
	bool SetupRecording(std::string* filename) { return false; }
	bool EndRecording() { return false; }
	void Configure() {}
	void Close() {}
	void Shutdown() {}
	s32 Open() { return 0; }

	s32 Init() { return 0; }

	s32 Freeze(FreezeAction mode, freezeData* data) { return 0; }
	void SetOutputPaused(bool paused) {}
} // namespace SPU
