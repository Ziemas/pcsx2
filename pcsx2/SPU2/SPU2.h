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

#include <array>
#include <string>
#include "common/Pcsx2Types.h"
#include "SaveState.h"

namespace SPU
{
	enum class PS2Modes
	{
		PS2,
		PSX,
	};

	void Run(u32 cycles);
	void RunDMA4();
	void RunDMA7();
	void InterruptDMA4();
	void InterruptDMA7();
	void WriteDMA(u32 channel, u16* madr, u32 size);
	void ReadDMA(u32 channel, u16* madr, u32 size);
	u16 Read(u32 addr);
	void Write(u32 addr, u16 value);
	void Reset(PS2Modes isRunningPSXMode);
	bool SetupRecording(std::string* filename);
	bool EndRecording();
	void Configure();
	void Close();
	void Shutdown();
	s32 Open(PS2Modes mode = PS2Modes::PS2);
	bool IsRunningPSXMode();

	s32 Freeze(FreezeAction mode, freezeData* data);
	//void FreezeIn(pxInputStream& reader);
	//void FreezeOut(void *dest);
	s32 Init();
	void SetOutputPaused(bool paused);

} // namespace SPU
