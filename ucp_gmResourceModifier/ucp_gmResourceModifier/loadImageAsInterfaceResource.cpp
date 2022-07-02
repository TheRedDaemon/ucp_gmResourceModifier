
#include "pch.h"

#include <wincodec.h>

#include "gmResourceModifierInternal.h"

#include "IUnknownWrapper.h"


// test
#include <fstream>


enum TgxStreamHeader : unsigned char
{
  STREAM_OF_PIXELS = 0x0,
  TGX_PIXEL_LENGTH = 0x1f,
  TRANSPARENT_PIXELS = 0x20,
  REPEATING_PIXELS = 0x40,
  NEWLINE = 0x80,
  TGX_PIXEL_HEADER = 0xe0,
};


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
  if (FAILED(imageFactory->CreateDecoderFromFilename(
    filepathWideStr,                    // Image to be decoded
    NULL,                               // Do not prefer a particular vendor
    GENERIC_READ,                       // Desired read access to the file
    WICDecodeMetadataCacheOnDemand,     // Cache metadata when needed
    decoderInterface.expose()           // Pointer to the decoder
  )))
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to obtain decoder. File invalid or missing.");
    return -1;
  }

  size_t frameCount{};
  decoderInterface->GetFrameCount(&frameCount);
  // TODO: use frameCount, to produce resource with all contained pictures

  size_t width{ 0 };
  size_t height{ 0 };
  std::vector<unsigned short> rawPixel{};

  // use blocks, so that the resources are closed after receiving the pixels
  {
    // Retrieve the first frame of the image from the decoder
    IUnknownWrapper<IWICBitmapFrameDecode> frame;
    if (FAILED(decoderInterface->GetFrame(0, frame.expose())))
    {
      LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to obtain frame.");
      return -1;
    }

    IUnknownWrapper<IWICBitmapSource> frameBGRA16;
    if (FAILED(WICConvertBitmapSource(GUID_WICPixelFormat16bppBGRA5551, frame.get(), frameBGRA16.expose())))
    {
      LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to transform an image into intermediate format.");
      return -1;
    }

    if (FAILED(frameBGRA16->GetSize(&width, &height)))
    {
      LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to obtain frame size.");
      return -1;
    }

    // I know the pixels are 16 bit, because I requested it
    rawPixel.resize(width * height);
    if (FAILED(frameBGRA16->CopyPixels(nullptr, width * sizeof(unsigned short), width * height * sizeof(unsigned short), (BYTE*)rawPixel.data())))
    {
      LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to obtain raw pixels.");
      return -1;
    }
  }


//{
//  size_t tgxSize{ 0 };
//  std::vector<unsigned char> rawTgx{};
//  rawTgx.resize(rawPixel.size() * sizeof(unsigned short) * 1.5f); // worst size is one control + one pixel -> number of pixels * size of ushort * 1.5
//  size_t counter{ 0 };
//  size_t lineCounter{};
//  bool transparent{ false };
//  bool sameColor{ false };
//  unsigned short lastColor{};
//
//  // transparency is broken
//  for (size_t i{ 0 }; i < rawPixel.size(); ++i)
//  {
//    unsigned short pixel{ rawPixel[i] };
//    if (pixel & 0x8000)
//    {
//      if (transparent)
//      {
//        while (counter > 0)
//        {
//          size_t transparentSize{ (counter > 32 ? 32 : counter) };
//          counter -= transparentSize;
//          rawTgx[tgxSize] = (unsigned char)(TRANSPARENT_PIXELS | (transparentSize - 1));
//          ++tgxSize;
//        }
//        transparent = false;
//      }
//
//      unsigned short checkColor{ pixel };
//      if (counter == 0)
//      {
//        sameColor = true;
//        lastColor = checkColor;
//      }
//      else if (lastColor != checkColor)
//      {
//        sameColor = false;
//      }
//
//      ++counter;
//      if (counter >= 32 || i == rawPixel.size() - 1)
//      {
//        if (sameColor)
//        {
//          rawTgx[tgxSize] = (unsigned char)(REPEATING_PIXELS | (counter - 1));
//          *((unsigned short*)&rawTgx[tgxSize + 1]) = lastColor;
//          tgxSize += 3;
//          counter = 0;
//          sameColor = false;
//        }
//        else // other color
//        {
//          rawTgx[tgxSize] = (unsigned char)(STREAM_OF_PIXELS | (counter - 1));
//          ++tgxSize;
//          for (int j{ (int)counter - 1 }; j >= 0; --j)
//          {
//            unsigned short color{ rawPixel[i - j] };
//            *((unsigned short*)&rawTgx[tgxSize]) = color;
//            tgxSize += 2;
//          }
//          counter = 0;
//        }
//      }
//    }
//    else // transparent else
//    {
//      if (!transparent)
//      {
//        if (counter > 0)
//        {
//          if (sameColor)
//          {
//            rawTgx[tgxSize] = (unsigned char)(REPEATING_PIXELS | (counter - 1));
//            *((unsigned short*)&rawTgx[tgxSize + 1]) = lastColor;
//            tgxSize += 3;
//            counter = 0;
//            sameColor = false;
//          }
//          else // other color
//          {
//            rawTgx[tgxSize] = (unsigned char)(STREAM_OF_PIXELS | (counter - 1));
//            ++tgxSize;
//            for (int j{ (int)counter - 1 }; j >= 0; --j)
//            {
//              unsigned short color{ rawPixel[i - j] };
//              *((unsigned short*)&rawTgx[tgxSize]) = color;
//              tgxSize += 2;
//            }
//            counter = 0;
//          }
//        }
//        transparent = true;
//      }
//      ++counter;
//    }
//
//    ++lineCounter;
//    if (lineCounter == width)
//    {
//      if (transparent)
//      {
//        //rawTgx[tgxSize] = NEWLINE;
//        //++tgxSize;
//        //transparent = false;
//        //counter = 0;
//        while (counter > 0)
//        {
//          size_t transparentSize{ (counter > 32 ? 32 : counter) };
//          counter -= transparentSize;
//          rawTgx[tgxSize] = (unsigned char)(TRANSPARENT_PIXELS | (transparentSize - 1));
//          ++tgxSize;
//        }
//        transparent = false;
//      }
//
//      if (i != height * width - 1)
//      {
//        rawTgx[tgxSize] = NEWLINE;  // nope
//        ++tgxSize;
//        lineCounter = 0;
//      }
//    }
//  }
//}

  size_t tgxSize{ 0 };
  std::vector<unsigned char> rawTgx{};
  // worst size is one control + one pixel + one newline at every vertical line end -> number of pixels * size of ushort * 1.5 + height
  rawTgx.resize(rawPixel.size() * sizeof(unsigned short) * 1.5f + height);
  
  // second try -> newline does apparently not work in all cases, some require transparent pixel before

  size_t lineCounter{};
  for (size_t i{ 0 }; i < rawPixel.size(); ++i)
  {
    size_t counter{ 0 };
    
    ++lineCounter;
    unsigned short pixel = rawPixel[i];
    if (!(pixel & 0x8000))  // transparent
    {
      ++counter;
      while (lineCounter < width && counter < 32)
      {
        pixel = rawPixel[i + 1];
        if (pixel & 0x8000)  // not transparent anymore
        {
          break;
        }
        ++counter;

        ++i;  // manipulate loop
        ++lineCounter;
      }
      rawTgx[tgxSize] = (unsigned char)(TRANSPARENT_PIXELS | (counter - 1));
      ++tgxSize;
    }
    else
    {
      unsigned short stateArray[32];
      stateArray[0] = pixel;
      ++counter;

      while (lineCounter < width && counter < 32)
      {
        pixel = rawPixel[i + 1];
        if (pixel != stateArray[counter - 1])  // not same color anymore
        {
          break;
        }
        stateArray[counter] = pixel;
        ++counter;

        ++i;  // manipulate loop
        ++lineCounter;
      }

      if (counter > 1)  // currently two pixel provoke same color
      {
        rawTgx[tgxSize] = (unsigned char)(REPEATING_PIXELS | (counter - 1));
        *((unsigned short*)&rawTgx[tgxSize + 1]) = stateArray[counter - 1];
        tgxSize += 3;
      }
      else
      {
        size_t sameCounter{};

        while (lineCounter < width && counter < 32)
        {
          pixel = rawPixel[i + 1];
          if (pixel == stateArray[counter - 1])  // same color
          {
            ++sameCounter;
            if (sameCounter > 0) // currently one repeating pixel provokes same color
            {
              // resetting index (one pixel at the moment!)
              counter -= 1;

              i -= 1;
              lineCounter -= 1;
              break;
            }
          }
          stateArray[counter] = pixel;
          ++counter;

          ++i;  // manipulate loop
          ++lineCounter;
        }

        rawTgx[tgxSize] = (unsigned char)(STREAM_OF_PIXELS | (counter - 1));
        ++tgxSize;
        std::memcpy(&rawTgx[tgxSize], stateArray, counter * sizeof(unsigned short));
        tgxSize += counter * sizeof(unsigned short);
      }
    }


    if (lineCounter >= width)
    {
      rawTgx[tgxSize] = NEWLINE;
      ++tgxSize;
      lineCounter = 0;
    }
  }


  // testout
  std::ofstream outdata;
  outdata.open("gm/test.tgx");
  outdata.write((char *) &width, 4);
  outdata.write((char*) &height, 4);
  outdata.write((char*) rawTgx.data(), tgxSize);

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