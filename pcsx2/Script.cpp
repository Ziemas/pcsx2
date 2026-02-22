// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/Console.h"
#include "Script.h"
#include "DebugTools/DebugInterface.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

namespace Script
{
	std::unordered_map<u32, std::vector<int>> eeHooks;

	static lua_State* L = nullptr;

	namespace Functions
	{
		// adapted from lbaselib.c
		static int console_print(lua_State* L)
		{
			int n = lua_gettop(L); /* number of arguments */
			int i;
			std::string out;

			for (i = 1; i <= n; i++)
			{ /* for each argument */
				size_t l;
				const char* s = luaL_tolstring(L, i, &l); /* convert it to string */
				if (i > 1) /* not the first element? */
					out.append("\t");

				out.append(s, l); /* print it */
				lua_pop(L, 1); /* pop result */
			}

			Console.WriteLn(out);
			return 0;
		}

		static int jit_hook(lua_State* L)
		{
			u32 addr = luaL_checkinteger(L, 1);

			if (!lua_isfunction(L, 2))
			{
				lua_pushliteral(L, "incorrect argument");
				Console.WriteLn("bad");
				lua_error(L);
			}

			lua_pushvalue(L, 2);
			int ref = luaL_ref(L, LUA_REGISTRYINDEX);
			Console.WriteLn("ref %d", ref);

			eeHooks[addr].push_back(ref);

			return 0;
		}

		static int read32(lua_State* L)
		{
			u32 a = luaL_checkinteger(L, 1);
			lua_pushinteger(L, r5900Debug.read32(a));

			return 1;
		}

		static int read_gpr(lua_State* L)
		{
			int regno = luaL_checkinteger(L, 1);
			lua_pushinteger(L, r5900Debug.getRegister(EECAT_GPR, regno)._u32[0]);

			return 1;
		}

		static const struct luaL_Reg globals[] = {
			{"print", console_print},
			{"hook", jit_hook},
			{"read32", read32},
			{"read_gpr", read_gpr},
			{NULL, NULL} /* end of array */
		};

		static void RegisterGlobals(lua_State* L)
		{
			lua_getglobal(L, "_G");
			luaL_setfuncs(L, globals, 0); // for Lua versions 5.2 or greater
			lua_pop(L, 1);
		}

	} // namespace Functions

	int Init()
	{
		if (L)
		{
			Shutdown();
		}

		L = luaL_newstate();
		luaL_openlibs(L);
		Functions::RegisterGlobals(L);

		luaL_loadfile(L, "test.lua");
		lua_pcall(L, 0, 0, 0);

		return 0;
	}

	void Shutdown()
	{
		if (L)
		{
			lua_close(L);
			L = nullptr;
		}
	}

	void SignalBoot()
	{
		int res;

		Console.WriteLn("lua boot");

		lua_getglobal(L, "on_boot");
		if (!lua_isfunction(L, -1))
		{
			Console.Error("missing function");
			return;
		}

		if ((res = lua_pcall(L, 0, 0, 0)) != LUA_OK)
		{
			Console.Error("Lua Error: %s", lua_tostring(L, -1));
		}
	}

	void SignalVSync()
	{
		int res;

		lua_getglobal(L, "vsync_callback");
		if (!lua_isfunction(L, -1))
		{
			lua_pop(L, 1);
			return;
		}

		if ((res = lua_pcall(L, 0, 0, 0)) != LUA_OK)
		{
			Console.Error("Lua Error: %s", lua_tostring(L, -1));
		}
	}

	bool HasHook(u32 addr)
	{
		return eeHooks.contains(addr);
	}

	void CallHooks(u32 addr)
	{
		for (auto ref : eeHooks[addr])
		{ 
			lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
			lua_pcall(L, 0, 0, 0);
		}
	}

} // namespace Script
