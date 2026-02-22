// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Types.h"
#include <unordered_map>
#include <vector>

namespace Script
{
	extern std::unordered_map<u32, std::vector<int>> eeHooks;

	int Init();
	void Shutdown();
	void SignalBoot();
	void SignalVSync();

	bool HasHook(u32 addr);
	void CallHooks(u32 addr);

} // namespace Script
