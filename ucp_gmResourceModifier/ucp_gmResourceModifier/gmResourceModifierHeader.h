
#ifndef GM_RESOURCE_MODIFIER_HEADER
#define GM_RESOURCE_MODIFIER_HEADER

#include <ucp3.h>

#include <lua.hpp>

namespace GmResourceModifierHeader
{
  // Cpp API

  using FuncLoadResource = int(__stdcall*)(const char* filepath);
  // reset is done by setting resource and image -1
  using FuncSetGm = bool(__stdcall*)(int gmID, int imageInGm, int resourceId, int imageInResource);
  using FuncFreeResource = bool(__stdcall*)(int resourceId);

  inline constexpr char const* NAME_VERSION{ "0.2.0" };
  inline constexpr char const* NAME_MODULE{ "gmResourceModifier" };
  inline constexpr char const* NAME_LIBRARY{ "gmResourceModifier.dll" };

  inline constexpr char const* NAME_LOAD_RESOURCE{ "_LoadGm1Resource@4" };
  inline constexpr char const* NAME_LOAD_RESOURCE_FROM_IMAGE{ "_LoadResourceFromImage@4" };
  inline constexpr char const* NAME_SET_GM{ "_SetGm@16" };
  inline constexpr char const* NAME_FREE_RESOURCE{ "_FreeGm1Resource@4" };

  inline FuncLoadResource LoadGm1Resource{ nullptr };
  inline FuncSetGm SetGm{ nullptr };
  inline FuncFreeResource FreeGm1Resource{ nullptr };
  inline FuncLoadResource LoadResourceFromImage{ nullptr };

  // returns true if the function variables of this header were successfully filled
  inline bool initModuleFunctions()
  {
    LoadGm1Resource = (FuncLoadResource) ucp_getProcAddressFromLibraryInModule(NAME_MODULE, NAME_LIBRARY, NAME_LOAD_RESOURCE);
    FreeGm1Resource = (FuncFreeResource) ucp_getProcAddressFromLibraryInModule(NAME_MODULE, NAME_LIBRARY, NAME_FREE_RESOURCE);
    LoadResourceFromImage = (FuncLoadResource) ucp_getProcAddressFromLibraryInModule(NAME_MODULE, NAME_LIBRARY, NAME_LOAD_RESOURCE_FROM_IMAGE);
    SetGm = (FuncSetGm) ucp_getProcAddressFromLibraryInModule(NAME_MODULE, NAME_LIBRARY, NAME_SET_GM);

    return LoadGm1Resource && SetGm && FreeGm1Resource && LoadResourceFromImage;
  }
}

#endif //GM_RESOURCE_MODIFIER_HEADER