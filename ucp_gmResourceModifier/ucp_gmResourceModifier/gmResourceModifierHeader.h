
#ifndef GM_RESOURCE_MODIFIER_HEADER
#define GM_RESOURCE_MODIFIER_HEADER

#include <lua.hpp>

namespace GmResourceModifierHeader
{
  // Cpp API

  using FuncLoadResource = int(__stdcall*)(const char* filepath);
  // reset is done by setting resource and image -1
  using FuncSetGm = bool(__stdcall*)(int gmID, int imageInGm, int resourceId, int imageInResource);
  using FuncFreeResource = bool(__stdcall*)(int resourceId);

  inline constexpr char const* NAME_VERSION{ "0.1.0" };

  inline constexpr char const* NAME_MODULE{ "gmResourceModifier" };
  inline constexpr char const* NAME_LOAD_RESOURCE{ "_LoadGm1Resource@4" };
  inline constexpr char const* NAME_SET_GM{ "_SetGm@16" };
  inline constexpr char const* NAME_FREE_RESOURCE{ "_FreeGm1Resource@4" };

  inline FuncLoadResource LoadGm1Resource{ nullptr };
  inline FuncSetGm SetGm{ nullptr };
  inline FuncFreeResource FreeGm1Resource{ nullptr };

  // returns true if the function variables of this header were successfully filled
  inline bool initModuleFunctions(lua_State* L)
  {
    if (LoadGm1Resource && SetGm && FreeGm1Resource) // assumed to not change during runtime
    {
      return true;
    }

    if (lua_getglobal(L, "modules") != LUA_TTABLE)
    {
      lua_pop(L, 1);  // remove value
      return false;
    }

    if (lua_getfield(L, -1, NAME_MODULE) != LUA_TTABLE)
    {
      lua_pop(L, 2);  // remove table and value
      return false;
    }

    LoadGm1Resource = (lua_getfield(L, -1, NAME_LOAD_RESOURCE) == LUA_TNUMBER) ? (FuncLoadResource)lua_tointeger(L, -1) : nullptr;
    lua_pop(L, 1);
    FreeGm1Resource = (lua_getfield(L, -1, NAME_FREE_RESOURCE) == LUA_TNUMBER) ? (FuncFreeResource)lua_tointeger(L, -1) : nullptr;
    lua_pop(L, 1);
    SetGm = (lua_getfield(L, -1, NAME_SET_GM) == LUA_TNUMBER) ? (FuncSetGm)lua_tointeger(L, -1) : nullptr;
    lua_pop(L, 3);  // remove value and all tables

    return LoadGm1Resource && SetGm && FreeGm1Resource;
  }
}

#endif //GM_RESOURCE_MODIFIER_HEADER