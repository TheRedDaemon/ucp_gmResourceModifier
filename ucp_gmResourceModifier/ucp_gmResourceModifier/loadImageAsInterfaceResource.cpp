
#include "pch.h"

#include <wincodec.h>

#include "gmResourceModifierInternal.h"

#include "IUnknownWrapper.h"


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

// hopefully moves
static std::vector<WCHAR> GetWideCharFilepathFromUTF8(const char* utf8Str)
{
  std::vector<WCHAR> wCharBuffer{};

  int sizeAndRes{ MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, 0, 0) };
  if (!sizeAndRes)
  {
    return {};
  }

  wCharBuffer.resize(sizeAndRes);

  sizeAndRes = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wCharBuffer.data(), sizeAndRes);
  if (!sizeAndRes)
  {
    return {};
  }

  return wCharBuffer;
}


// hopefully at least moves
static std::vector<unsigned short> GetFrameData(IUnknownWrapper<IWICBitmapDecoder>& decoderInterface,
  size_t frameIndex, size_t& outWidth, size_t& outHeight)
{
  std::vector<unsigned short> rawPixel{};
  size_t width{};
  size_t height{};

  // Retrieve the first frame of the image from the decoder
  IUnknownWrapper<IWICBitmapFrameDecode> frame;
  if (FAILED(decoderInterface->GetFrame(frameIndex, frame.expose())))
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to obtain frame.");
    return {};
  }

  IUnknownWrapper<IWICBitmapSource> frameBGRA16;
  if (FAILED(WICConvertBitmapSource(GUID_WICPixelFormat16bppBGRA5551, frame.get(), frameBGRA16.expose())))
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to transform an image into intermediate format.");
    return {};
  }

  if (FAILED(frameBGRA16->GetSize(&width, &height)))
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to obtain frame size.");
    return {};
  }

  // I know the pixels are 16 bit, because I requested it
  rawPixel.resize(width * height);
  if (FAILED(frameBGRA16->CopyPixels(nullptr, width * sizeof(unsigned short), width * height * sizeof(unsigned short), (BYTE*)rawPixel.data())))
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to obtain raw pixels.");
    return {};
  }

  outWidth = width;
  outHeight = height;
  return rawPixel;
}


// hopefully at least moves
static std::vector<unsigned char> TransformToTGX(const std::vector<unsigned short>& rawPixel, size_t width, size_t height)
{
  std::vector<unsigned char> rawTgx{};

  // worst size is one control + one pixel + one newline at every vertical line end -> number of pixels * size of ushort * 1.5 + height
  rawTgx.resize(rawPixel.size() * sizeof(unsigned short) * 1.5f + height);

  size_t tgxSize{ 0 };
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

  rawTgx.resize(tgxSize); // reduce to actual size
  return rawTgx;
}



// sources:
// https://docs.microsoft.com/en-us/windows/win32/wic/-wic-api
// https://docs.microsoft.com/de-de/windows/win32/wic/-wic-creating-decoder
int Gm1ResourceManager::CreateGm1ResourceFromImage(const char* filepath)
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

  std::vector<WCHAR> filepathWideStr{ GetWideCharFilepathFromUTF8(filepath) };
  if (filepathWideStr.empty())
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to decode filepath (UTF-8).");
    return -1;
  }

  // load image

  IUnknownWrapper<IWICBitmapDecoder> decoderInterface;
  if (FAILED(imageFactory->CreateDecoderFromFilename(
    filepathWideStr.data(),             // Image to be decoded
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
  if (FAILED(decoderInterface->GetFrameCount(&frameCount)))
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: LoadResourceFromImage: Was unable to frame count.");
    return -1;
  }

  std::vector<ImageHeader> resourceImageHeaders{};
  std::vector<int> resourceOffsets{};
  std::vector<int> resourceSizes{};
  std::vector<std::vector<unsigned char>> resourcesDataVectors{};

  for (size_t i{ 0 }; i < frameCount; ++i)
  {
    size_t width{ 0 };
    size_t height{ 0 };
    std::vector<unsigned short> rawPixel{ GetFrameData(decoderInterface, 0, width, height) };
    if (rawPixel.empty())
    {
      return -1;
    }

    resourcesDataVectors.push_back(TransformToTGX(rawPixel, width, height));
    if (resourcesDataVectors.back().empty())
    {
      return -1;
    }

    resourceOffsets.push_back( i > 0 ? resourceOffsets.back() + resourceSizes.back() : 0 );
    resourceSizes.push_back(resourcesDataVectors.back().size());
    ImageHeader& header{ resourceImageHeaders.emplace_back() };

    // for now only set dimensions?
    header.width = static_cast<unsigned short>(width);
    header.height = static_cast<unsigned short>(height);
  }

  // a little late, but I do not want to return the id at every if statement
  // also sadly duplicate from the other id load

  int newId{ GetId() };
  if (newId < 0)
  {
    LogHelper(LuaLog::LOG_WARNING, "[gmResourceModifier]: Error while loading resource ", filepath, "All ids used.");
    return -1;
  }

  auto [it, success] { resources.try_emplace(newId) };
  if (!success)
  {
    LogHelper(LuaLog::LOG_WARNING, "[gmResourceModifier]: Error while loading resource ", filepath, "Failed to place resource in map.");
    ReturnId(newId);
    return -1;
  }

  // create header

  Gm1Resource& resource{ it->second };

  // create full array
  resource.imageData.swap(resourcesDataVectors.at(0));
  if (resourcesDataVectors.size() > 1)
  {
    size_t totalSize{ resource.imageData.size() };
    size_t currentIndex{ totalSize };

    for (size_t i{ 1 }; i < resourcesDataVectors.size(); i++)
    {
      totalSize += resourcesDataVectors.at(i).size();
    }
    resource.imageData.resize(totalSize);

    for (size_t i{ 1 }; i < resourcesDataVectors.size(); i++)
    {
      auto& dataVec{ resourcesDataVectors.at(i) };
      std::memcpy(&resource.imageData[currentIndex], dataVec.data(), dataVec.size());
      currentIndex += dataVec.size();
    }
  }

  resource.gm1Header = std::make_unique<Gm1Header>();
  resource.gm1Header->gm1Type = Gm1Type::INTERFACE; // testing with interface
  resource.gm1Header->numberOfPicturesInFile = resourceImageHeaders.size();
  resource.gm1Header->dataSize = resource.imageData.size();

  resource.imageHeader.swap(resourceImageHeaders);
  resource.imageSizes.swap(resourceSizes);
  resource.imageOffset.swap(resourceOffsets);

  if (initDone && !Gm1ResourceManager::ReadyResource(resource))
  {
    LogHelper(LuaLog::LOG_WARNING, "[gmResourceModifier]: Error while loading resource ", filepath, "Failed to ready resource.");
    ReturnId(newId);
    resources.erase(newId);
    return -1;
  }

  resource.resourceId = newId;
  return newId;
}



extern "C" __declspec(dllexport) int __stdcall LoadResourceFromImage(const char* filepath)
{
  return Gm1ResourceManager::CreateGm1ResourceFromImage(filepath);
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