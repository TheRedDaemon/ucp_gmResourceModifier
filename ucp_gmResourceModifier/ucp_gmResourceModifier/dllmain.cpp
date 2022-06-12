
#include "pch.h"

// lua
#include "lua.hpp"

#include "gmResourceModifierInternal.h"

// lua module load
extern "C" __declspec(dllexport) int __cdecl luaopen_gmResourceModifier(lua_State * L)
{
  if (!LuaLog::init(L))
  {
    luaL_error(L, "[gmResourceModifier]: Failed to receive Log functions.");
  }

  lua_newtable(L); // push a new table on the stack

  // simple replace
  // get member func ptr, source: https://github.com/microsoft/Detours/blob/master/samples/member/member.cpp
  auto memberFuncPtr{ &ColorAdapter::detouredLoadGmFiles };
  lua_pushinteger(L, *(DWORD*)&memberFuncPtr);
  lua_setfield(L, -2, "funcAddress_DetouredLoadGmFiles");

  // address
  lua_pushinteger(L, (DWORD)&ColorAdapter::actualLoadGmsFunc);
  lua_setfield(L, -2, "address_ActualLoadGmsFunc");

  // address
  lua_pushinteger(L, (DWORD)&ColorAdapter::transformTgxFromRGB555ToRGB565);
  lua_setfield(L, -2, "address_TransformTgxFromRGB555ToRGB565");

  // address
  lua_pushinteger(L, (DWORD)&shcImageHeaderStart);
  lua_setfield(L, -2, "address_ShcImageHeaderStart");

  // address
  lua_pushinteger(L, (DWORD)&shcSizesStart);
  lua_setfield(L, -2, "address_ShcSizesStart");

  // address
  lua_pushinteger(L, (DWORD)&shcOffsetStart);
  lua_setfield(L, -2, "address_ShcOffsetStart");

  // add functions
  lua_newtable(L); // push function table
  lua_pushinteger(L, (DWORD)SetGm);
  lua_setfield(L, -2, GmResourceModifierHeader::NAME_SET_GM);
  lua_pushinteger(L, (DWORD)LoadGm1Resource);
  lua_setfield(L, -2, GmResourceModifierHeader::NAME_LOAD_RESOURCE);

  // add table
  lua_setfield(L, -2, "funcPtr");

  // return lua funcs

  lua_pushcfunction(L, lua_SetGm);
  lua_setfield(L, -2, "lua_SetGm");
  lua_pushcfunction(L, lua_LoadGm1Resource);
  lua_setfield(L, -2, "lua_LoadGm1Resource");

  return 1;
}

// entry point
BOOL APIENTRY DllMain(HMODULE,
  DWORD  ul_reason_for_call,
  LPVOID
)
{
  switch (ul_reason_for_call)
  {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
      break;
  }
  return TRUE;
}