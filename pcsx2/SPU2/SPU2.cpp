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
#include "common/MemcpyFast.h"
#include "Output.h"
#include "IopHw.h"

namespace SPU
{
	u16 SPU_RAM[1024 * 1024 * 2] = {};
	//std::array<u16, 1024*1024*2> SPU_RAM = {};

	SPUCore cores[2] = {
		{SPU_RAM, 0},
		{SPU_RAM, 1},
	};

	u32 spuCycles = 0;
	u32 dmaCycles = 0;

	std::unique_ptr<SndOutput> snd;

	FILE* output{nullptr};

	void Run(u32 cycles)
	{
		spuCycles += cycles;
		while (spuCycles >= 768)
		{
			auto core0 = cores[0].GenSample(AudioSample());
			auto core1 = cores[1].GenSample(core0);
			spuCycles -= 768;

			S16Out out{};
			out.left = core1.left;
			out.right = core1.right;
			//fwrite(&out, sizeof(S16Out), 1, output);

			snd->Push(out);
		}
	}

	void RunDMA4()
	{
		cores[0].RunDma();
	}

	void RunDMA7()
	{
		cores[1].RunDma();
	}

	void InterruptDMA4()
	{
		//Console.WriteLn("SPU irq dma4");
	}

	void InterruptDMA7()
	{
		//Console.WriteLn("SPU irq dma7");
	}

	void WriteDMA(u32 core, u16* madr, u32 size)
	{
		cores[core].DmaWrite(madr, size);
	}

	void ReadDMA(u32 core, u16* madr, u32 size)
	{
		cores[core].DmaRead(madr, size);
	}

	u16 Read(u32 addr)
	{
		if (addr >= 0x1F900800)
			return 0;

		addr &= 0x7FF;

		if (addr < 0x760)
		{
			u32 core = (addr >> 10) & 1;
			addr &= 0x3FF;
			return cores[core].Read(addr);
		}

        if (addr < 0x788) // core0 volume, reverb addrs
        {
			return cores[0].Read(addr);
        }

        if (addr < 0x7C0) // core1 volume, reverb addrs
		{
			addr -= 0x28;
            return cores[1].Read(addr);
		}

		if (addr >= 0x7C0) // shared?
		{
			return cores[0].Read(addr);
		}

		return 0;
	}

	void Write(u32 addr, u16 value)
	{
		if (addr >= 0x1F900800)
			return;

		addr &= 0x7FF;

		if (addr < 0x760)
		{
			u32 core = (addr >> 10) & 1;
			addr &= 0x3FF;
			cores[core].Write(addr, value);
			return;
		}

        if (addr < 0x788) // core0 volume, reverb addrs
        {
            cores[0].Write(addr, value);
            return;
        }

        if (addr < 0x7C0) // core1 volume, reverb addrs
        {
            addr -= 0x28;
            cores[1].Write(addr, value);
            return;
        }

		if (addr >= 0x7C0) // shared?
		{
            cores[0].Write(addr, value);
			return;
		}
	}

	void Reset(PS2Modes isRunningPSXMode)
	{
		for (auto& c : cores)
			c.Reset();

		memzero(SPU_RAM);

		snd->Clear();
		// Workaround fast boot breaking megaman x7
		// BIOS will leave a loop end point here
		SPU_RAM[0xe868] = 0x100;

		Console.WriteLn("SPU RESET");
		if (output != nullptr)
			fclose(output);
		output = fopen("output.pcm", "wb");
	}

	bool SetupRecording(std::string* filename)
	{
		return false;
	}

	bool EndRecording()
	{
		return false;
	}

	void Configure()
	{
		auto f = fopen("spumem", "wb");
		fwrite(SPU_RAM, 1024*1024*2, 1, f);
	}

	void Close()
	{
		Console.WriteLn("SPU CLOSE");
	}

	void Shutdown()
	{
		snd.reset();
	}

	s32 Open(PS2Modes mode)
	{
		Console.WriteLn("SPU OPEN");
		return 0;
	}

	s32 Init()
	{
		snd = std::make_unique<SndOutput>();
		return 0;
	}

	s32 Freeze(FreezeAction mode, freezeData* data)
	{
		return 0;
	}

	bool IsRunningPSXMode() { return false; };

	void SetOutputPaused(bool paused) {}

	void SetDeviceSampleRateMultiplier(double multiplier) {}

	const char* const* GetOutputModuleBackends(const char* omodid) { return nullptr; }

} // namespace SPU
