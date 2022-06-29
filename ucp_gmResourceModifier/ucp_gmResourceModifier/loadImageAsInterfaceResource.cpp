
#include "pch.h"

#include <wincodec.h>

#include "gmResourceModifierInternal.h"

#include "IUnknownWrapper.h"


static IUnknownWrapper<IWICImagingFactory> imageFactory;


// WARNING: not thread safe
static LPCWSTR GetWideCharFilepathFromUTF8(const char* utf8Str)
{
  static std::vector<WCHAR> wCharBuffer{};  // stays to return ptr

  int sizeAndRes{ MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, 0, 0) };
  if (!sizeAndRes)
  {
    return nullptr;
  }

  wCharBuffer.resize(sizeAndRes);

  sizeAndRes = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wCharBuffer.data(), sizeAndRes);
  if (!sizeAndRes)
  {
    return nullptr;
  }

  return wCharBuffer.data();
}


// sources:
// https://docs.microsoft.com/en-us/windows/win32/wic/-wic-api
// https://docs.microsoft.com/de-de/windows/win32/wic/-wic-creating-decoder
extern "C" __declspec(dllexport) int __stdcall LoadResourceFromImage(const char* filepath)
{
  if (!imageFactory)
  {
    if (CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(imageFactory.expose())) != S_OK)
    {
      LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to obtain image factory interface.");
      return -1;
    }
  }

  // get filepath

  LPCWSTR filepathWideStr{ GetWideCharFilepathFromUTF8(filepath) };
  if (!filepathWideStr)
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to decode filepath (UTF-8).");
    return -1;
  }

  // load image

  IUnknownWrapper<IWICBitmapDecoder> decoderInterface;

  HRESULT hr{ imageFactory->CreateDecoderFromFilename(
    filepathWideStr,                    // Image to be decoded
    NULL,                               // Do not prefer a particular vendor
    GENERIC_READ,                       // Desired read access to the file
    WICDecodeMetadataCacheOnDemand,     // Cache metadata when needed
    decoderInterface.expose()           // Pointer to the decoder
  ) };

  if (FAILED(hr))
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to obtain decoder.");
    return -1;
  }

  size_t frameCount{};
  decoderInterface->GetFrameCount(&frameCount);
  // TODO: use frameCount, to produce resource with all contained pictures

  // Retrieve the first frame of the image from the decoder
  IUnknownWrapper<IWICBitmapFrameDecode> frame;
  if (FAILED(decoderInterface->GetFrame(0, frame.expose())))
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to obtain frame.");
    return -1;
  }

  // use this further: https://stackoverflow.com/a/52685194
  return 0;
}


// lua

extern "C" __declspec(dllexport) int __cdecl lua_LoadResourceFromImage(lua_State * L)
{
  int n{ lua_gettop(L) };    /* number of arguments */
  if (n != 1)
  {
    luaL_error(L, "[gmResourceModifier]: lua_LoadResourceFromImage: Invalid number of args.");
  }

  if (!lua_isstring(L, 1))
  {
    luaL_error(L, "[gmResourceModifier]: lua_LoadResourceFromImage: Wrong input fields.");
  }

  int res{ LoadResourceFromImage(lua_tostring(L, 1)) };
  lua_pushinteger(L, res);
  return 1;
}