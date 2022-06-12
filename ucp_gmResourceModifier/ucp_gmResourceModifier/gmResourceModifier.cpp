#pragma once

#include "pch.h"

#include "gmResourceModifierInternal.h"

#include <vector>
#include <unordered_map>

#include <ios>
#include <fstream>
#include <sstream>


// static variables:

static std::vector<Replacer> replacerVector{};
static std::unordered_map<int, Gm1Resource> resources{};
static std::vector<int> freeIds{};

static std::vector<std::tuple<int, int, int, int>> preRequests{}; // contains replace requests made before all variables were initialized
static bool initDone{ false };

// variables received from the header:
static Gm1Header* shcHeaderStart{};
static int shcGmDataAddr{};
static BOOL shcOffsetModifierFlag{};

// used for init
void __thiscall ColorAdapter::detouredLoadGmFiles(char* fileNameArray)
{
  if (initDone)
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier] Called init func again. Ignored.");
    return;
  }

  // call actual load gms func:
  shcTransformStruct = this;  // set ptr
  (*shcTransformStruct.*actualLoadGmsFunc)(fileNameArray);  // actual init everything

  // get gmheader, because they seem to be a member variable (currently from SHC 1.41)
  shcHeaderStart = (Gm1Header*)(((unsigned char*) shcTransformStruct) + 1308);

  // get the memory address of the processed data, because they seem to be a member variable (currently from SHC 1.41)
  shcGmDataAddr = *(int*)(((unsigned char*)shcTransformStruct) + 120);

  // needed to adjust image offset and sizes (although, the whole data is still kept in memory) (currently from SHC 1.41)
  shcOffsetModifierFlag = *(BOOL*)(((unsigned char*)shcTransformStruct) + 80);

  initDone = true;  // needed here to ready resources

  // ready everything:

  // hardcoded to 240 headers currently
  size_t numberOfImages{ 0 };
  for (size_t i{ 0 }; i < 240; i++)
  {
    int pictureIndex{ static_cast<int>(shcHeaderStart[i].numberOfPicturesInFile < 1 ? -1 : numberOfImages) };
    replacerVector.emplace_back(i, pictureIndex);
    numberOfImages += shcHeaderStart[i].numberOfPicturesInFile;
  }

  for (auto& [resourceId, resource] : resources)
  {
    Gm1Resource::ReadyResource(resource);
  }

  for (const auto& [gmId, imageInGm, resourceId, imageInResource] : preRequests)
  {
    SetGm(gmId, imageInGm, resourceId, imageInResource); // need to call replace in order of definition
  }
  preRequests.clear();
}


void ColorAdapter::TransformRGB555ToRGB565(unsigned short* data, size_t byteSize, TransformType transformType)
{
  size_t loopCount{ byteSize / 2 };
  
  switch (transformType)
  {
    case TransformType::UNCOMPRESSED_2:
    {
      for (size_t i{ 0 }; 0 < loopCount; ++i)
      {
        if (data[i] != 0b1111100000011111)
        {
          data[i] = (data[i] & 0x1f) + ((data[i] & 0x3e0) >> 5) * 0x40 + ((data[i] & 0x7c00) >> 10) * 0x800;
        }
      }
      return;
    }

    case TransformType::COLOR_PALETTE:
    {
      for (size_t i{ 0 }; 0 < loopCount; ++i)
      {
        data[i] = (data[i] & 0x1f) + ((data[i] & 0xfc00) + (data[i] & 0x3e0)) * 2;
      }
      return;
    }

    default:
    {
      for (size_t i{ 0 }; 0 < loopCount; ++i)
      {
        data[i] = (data[i] & 0x1f) + ((data[i] & 0x3e0) >> 5) * 0x40 + ((data[i] & 0x7c00) >> 10) * 0x800;
      }
      return;
    }
  }
}


void ColorAdapter::AdaptGm1Resource(Gm1Resource& resource)
{
  if (resource.done)
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier] Color adapt received already done resource. Skipping transform.");
  }

  if (*gamePixelFormat != PixelFormat::RGB_565) {
    return;
  }

  switch (resource.gm1Header->gm1Type)
  {
    case Gm1Type::ANIMATIONS:
      TransformRGB555ToRGB565((unsigned short*) resource.gm1Header->colorPalette, sizeof(resource.gm1Header->colorPalette), TransformType::COLOR_PALETTE);
      break;

    case Gm1Type::INTERFACE:
    case Gm1Type::FONT:
    case Gm1Type::TGX_CONST_SIZE:
      (*shcTransformStruct.*transformTgxFromRGB555ToRGB565)(resource.imageData.get(), resource.gm1Header->dataSize);
      break;

    case Gm1Type::NO_COMPRESSION_1:
      TransformRGB555ToRGB565((unsigned short*) resource.imageData.get(), resource.gm1Header->dataSize, TransformType::NORMAL);
      break;

    case Gm1Type::NO_COMPRESSION_2:
      TransformRGB555ToRGB565((unsigned short*) resource.imageData.get(), resource.gm1Header->dataSize, TransformType::UNCOMPRESSED_2);
      break;

    case Gm1Type::TILES_OBJECT:
    {
      for (size_t i{ 0 }; i < resource.gm1Header->numberOfPicturesInFile; ++i)
      {
        // offset needs to be relative to file for this, so offset adjust needs to be last step
        unsigned short* dataPtr{ (unsigned short*) (resource.imageData.get() + resource.imageOffset[i]) };
        TransformRGB555ToRGB565(dataPtr, 512, TransformType::NORMAL);
        if (resource.imageHeader[i].direction != 0)
        {
          // adjusted ptr value add for short
          (*shcTransformStruct.*transformTgxFromRGB555ToRGB565)(dataPtr + 256, resource.imageSizes[i] - 512);
        }
      }
      break;
    }

    default:
      LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier] Color adapt received unknown resource type. Skipping transform.");
      break;
  }
}


void Gm1Resource::LogHelper(LuaLog::LogLevel level, const char* start, const char* filename, const char* error)
{
  static std::stringstream stream{};
  stream.clear();
  stream << start << filename << "': " << error;
  LuaLog::log(level, stream.str().c_str());
}

int Gm1Resource::CreateGm1Resource(const char* filename)
{
  static int idGiver{ 0 };
  
  int newId{ 0 };
  if (!freeIds.empty())
  {
    newId = freeIds.back();
    freeIds.pop_back();
  }
  else if (idGiver != INT_MAX)
  {
    newId = idGiver++;
  }
  else
  {
    LogHelper(LuaLog::LOG_WARNING, "[gmResourceModifier]: Error while loading resource ", filename, "All ids used.");
    return -1;
  }

  // create temp
  std::unique_ptr<Gm1Header> gm1HeaderTemp;
  std::unique_ptr<int[]> imageOffsetTemp;
  std::unique_ptr<int[]> imageSizesTemp;
  std::unique_ptr<ImageHeader[]> imageHeaderTemp;
  std::unique_ptr<unsigned char[]> imageDataTemp;

  // source: https://stackoverflow.com/a/17338119
  std::ifstream ifs;
  //prepare ifs to throw if failbit gets set
  ifs.exceptions(std::ios::failbit | std::ios::badbit);
  try
  {
    ifs.open(filename, std::ios::binary);

    gm1HeaderTemp = std::make_unique<Gm1Header>();
    ifs.read((char*) gm1HeaderTemp.get(), sizeof(Gm1Header));

    int numberOfPictures{ static_cast<int>(gm1HeaderTemp->numberOfPicturesInFile) };

    imageOffsetTemp = std::make_unique<int[]>(numberOfPictures);
    ifs.read((char*) imageOffsetTemp.get(), numberOfPictures * sizeof(int));

    imageSizesTemp = std::make_unique<int[]>(numberOfPictures);
    ifs.read((char*)imageSizesTemp.get(), numberOfPictures * sizeof(int));

    imageHeaderTemp = std::make_unique<ImageHeader[]>(numberOfPictures);
    ifs.read((char*)imageHeaderTemp.get(), numberOfPictures * sizeof(ImageHeader));

    imageDataTemp = std::make_unique<unsigned char[]>(gm1HeaderTemp->dataSize);
    ifs.read((char*) imageDataTemp.get(), gm1HeaderTemp->dataSize);

    ifs.close();  // closing manually to trigger error here and not later
  }
  catch (std::ios_base::failure& e)
  {
    char errMsg[100];
    strerror_s(errMsg, 100, errno);
    LogHelper(LuaLog::LOG_WARNING, "[gmResourceModifier]: Error while loading resource ", filename, errMsg);
    freeIds.push_back(newId);
    return -1;
  }

  auto [it, success]{ resources.try_emplace(newId) };
  if (!success)
  {
    LogHelper(LuaLog::LOG_WARNING, "[gmResourceModifier]: Error while loading resource ", filename, "Failed to place resource in map.");
    freeIds.push_back(newId);
    return -1;
  }
  
  Gm1Resource& resource{ it->second };
  resource.gm1Header.swap(gm1HeaderTemp);
  resource.imageHeader.swap(imageHeaderTemp);
  resource.imageSizes.swap(imageSizesTemp);
  resource.imageOffset.swap(imageOffsetTemp);
  resource.imageData.swap(imageDataTemp);

  if (initDone && !Gm1Resource::ReadyResource(resource))
  {
    LogHelper(LuaLog::LOG_WARNING, "[gmResourceModifier]: Error while loading resource ", filename, "Failed to ready resource.");
    freeIds.push_back(newId);
    resources.erase(newId);
    return -1;
  }

  resource.resourceId = newId;
  return newId;
}

bool Gm1Resource::ReadyResource(Gm1Resource& resource)
{
  if (!initDone)
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: Tried to ready resource before init.");
    return false;
  }

  if (resource.done)
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: Tried to ready resource twice.");
    return false;
  }

  ColorAdapter::AdaptGm1Resource(resource); // needs relative to object offsets

    // adjust offsets to SHC memory
  for (size_t i{ 0 }; i < resource.gm1Header->numberOfPicturesInFile; ++i)
  {
    resource.imageOffset[i] = (int)resource.imageData.get() + resource.imageOffset[i] - shcGmDataAddr;

    // tries to recreate stuff used by the game, but without resource copy -> it will not save memory space
    if (shcOffsetModifierFlag == 0)
    {
      if ((resource.imageHeader[i].animatedColor & 4) != 0)
      {
        resource.imageOffset[i] = resource.imageOffset[i - 1];
        resource.imageSizes[i] = resource.imageSizes[resource.imageHeader[i].relativeDataPos + i];
      }
    }
    else if (resource.imageHeader[i].animatedColor != 0)
    {
      resource.imageOffset[i] = (resource.imageHeader[i].animatedColor & 4) != 0 ? resource.imageOffset[i - 1] 
        : resource.imageOffset[resource.imageHeader[i].relativeDataPos + i];
      resource.imageSizes[i] = resource.imageSizes[resource.imageHeader[i].relativeDataPos + i];
    }
  }

  resource.done = true;
  return true;
}

void Replacer::readyOrigResource()
{
  if (origResource)
  {
    return; // already done
  }

  origResource = std::make_unique<Gm1Resource>();
  origResource->gm1Header = std::make_unique<Gm1Header>();
  std::memcpy(origResource->gm1Header.get(), &shcHeaderStart[gmIndex], sizeof(Gm1Header));

  int numberOfPictures{ static_cast<int>(origResource->gm1Header->numberOfPicturesInFile) };

  if (numberOfPictures < 1)
  {
    return; // we are done here, there is nothing in this index
  }

  // copy data
  origResource->imageOffset = std::make_unique<int[]>(numberOfPictures);
  std::memcpy(origResource->imageOffset.get(), &shcOffsetStart[firstImageIndex], numberOfPictures * sizeof(int));

  origResource->imageSizes = std::make_unique<int[]>(numberOfPictures);
  std::memcpy(origResource->imageSizes.get(), &shcSizesStart[firstImageIndex], numberOfPictures * sizeof(int));

  origResource->imageHeader = std::make_unique<ImageHeader[]>(numberOfPictures);
  std::memcpy(origResource->imageHeader.get(), &shcImageHeaderStart[firstImageIndex], numberOfPictures * sizeof(ImageHeader));

  origResource->done = true;
}

// no error checks here
void Replacer::copyToShc(Gm1Resource& resource, int imageIndex, int resourceImageIndex)
{
  if (imageIndex == -1)
  {
    int numberOfPictures{ static_cast<int>(resource.gm1Header->numberOfPicturesInFile) };

    std::memcpy(&shcHeaderStart[gmIndex], resource.gm1Header.get(), sizeof(Gm1Header));

    int otherImagesReplacedCounter{ 0 };
    for (const auto& [imageI, resourcePair] : imageReplaced)
    {
      // index of image
      --(resources.at(resourcePair.first).refCounter);
      ++otherImagesReplacedCounter;
    }
    imageReplaced.clear();

    if (mainReplacingResource)
    {
      mainReplacingResource->refCounter -= (numberOfPictures + 1 - otherImagesReplacedCounter); // + 1 for header
    }

    if (origResource.get() != &resource)
    {
      resource.refCounter += (numberOfPictures + 1);  // + 1 for header
      mainReplacingResource = &resource;
    }
    else
    {
      mainReplacingResource = nullptr;
    }
    
    std::memcpy(&shcOffsetStart[firstImageIndex], resource.imageOffset.get(), numberOfPictures * sizeof(int));
    std::memcpy(&shcSizesStart[firstImageIndex], resource.imageSizes.get(), numberOfPictures * sizeof(int));
    std::memcpy(&shcImageHeaderStart[firstImageIndex], resource.imageHeader.get(), numberOfPictures * sizeof(ImageHeader));

    return;
  }

  auto iter{ imageReplaced.find(imageIndex) };
  if (iter == imageReplaced.end())
  {
    if (mainReplacingResource)
    {
      --(mainReplacingResource->refCounter);
    }

    if (origResource.get() != &resource)
    {
      imageReplaced.try_emplace(imageIndex, resource.resourceId, resourceImageIndex);
    }
  }
  else
  {
    if (origResource.get() != &resource)
    {
      --(resources.at(iter->second.first).refCounter);
      iter->second = std::pair<int, int>(resource.resourceId, resourceImageIndex);
    }
  }

  if (origResource.get() != &resource)
  {
    ++(resource.refCounter);
  }

  shcOffsetStart[firstImageIndex + imageIndex] = resource.imageOffset[resourceImageIndex];
  shcSizesStart[firstImageIndex + imageIndex] = resource.imageSizes[resourceImageIndex];
  shcImageHeaderStart[firstImageIndex + imageIndex] = resource.imageHeader[resourceImageIndex];
}


/* export C */

extern "C" __declspec(dllexport) int __stdcall LoadGm1Resource(const char* filepath)
{
  return Gm1Resource::CreateGm1Resource(filepath);
}


extern "C" __declspec(dllexport) bool __stdcall SetGm(int gmID, int imageInGm, int resourceId, int imageInResource)
{
  if (!initDone)
  {
    static bool warnPrinted{ false };
    if (!warnPrinted)
    {
      LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: Important: 'SetGm' Requests before the first Init will always return true.");
      warnPrinted = true;
    }

    preRequests.emplace_back(gmID, imageInGm, resourceId, imageInResource);
    return true;
  }

  if (gmID < 0 || gmID > 239) // hardcoded currently
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: SetGm: Invalid GmID given.");
    return false;
  }

  Replacer& currentReplacer{ replacerVector[gmID] };
  currentReplacer.readyOrigResource();

  if (imageInGm >= 0 && imageInGm >= currentReplacer.origResource->gm1Header->numberOfPicturesInFile - 1)
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: SetGm: Invalid SHC GM1 picture number given.");
    return false;
  }

  if (resourceId < 0) // indicator for resetting -> imageInResource ignored
  {
    if (imageInGm < 0)
    {
      currentReplacer.copyToShc(*currentReplacer.origResource, -1, -1); // reset everything
    }
    else
    {
      currentReplacer.copyToShc(*currentReplacer.origResource, imageInGm, imageInGm); // reset an image
    }
    return true;
  }

  auto iter{ resources.find(resourceId) };
  if (iter == resources.end())
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: SetGm: Invalid ResourceID given.");
    return false;
  }
  Gm1Resource& resource{ iter->second };

  if (currentReplacer.origResource->gm1Header->gm1Type != resource.gm1Header->gm1Type)
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: SetGm: Resources can only be replaced by the same type.");
    return false;
  }

  if (imageInResource < 0)
  {
    if (currentReplacer.origResource->gm1Header->numberOfPicturesInFile != resource.gm1Header->numberOfPicturesInFile)
    {
      LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: SetGm: Complete resources replace can only happen if number of images equal.");
      return false;
    }

    currentReplacer.copyToShc(resource, -1, -1);
    return true;
  }

  if (imageInResource > resource.gm1Header->numberOfPicturesInFile - 1)
  {
    LuaLog::log(LuaLog::LOG_WARNING, "[gmResourceModifier]: SetGm: Invalid resource number given.");
    return false;
  }

  currentReplacer.copyToShc(resource, imageInGm, imageInResource);
  return true;
}


/* export LUA */

extern "C" __declspec(dllexport) int __cdecl lua_LoadGm1Resource(lua_State * L)
{
  int n{ lua_gettop(L) };    /* number of arguments */
  if (n != 1)
  {
    luaL_error(L, "[textResourceModifier]: lua_LoadGm1Resource: Invalid number of args.");
  }

  if (!lua_isstring(L, 1))
  {
    luaL_error(L, "[textResourceModifier]: lua_LoadGm1Resource: Wrong input fields.");
  }

  int res{ Gm1Resource::CreateGm1Resource(lua_tostring(L, 1)) };
  lua_pushinteger(L, res);
  return 1;
}


extern "C" __declspec(dllexport) int __cdecl lua_SetGm(lua_State * L)
{
  int n{ lua_gettop(L) };    /* number of arguments */
  if (n != 4)
  {
    luaL_error(L, "[textResourceModifier]: lua_SetGm: Invalid number of args.");
  }

  if (!(lua_isinteger(L, 1) && lua_isinteger(L, 2) && lua_isinteger(L, 3) && lua_isinteger(L, 4)))
  {
    luaL_error(L, "[textResourceModifier]: lua_SetGm: Wrong input fields.");
  }

  bool res{ SetGm(lua_tointeger(L, 1), lua_tointeger(L, 2), lua_tointeger(L, 3), lua_tointeger(L, 4)) };
  lua_pushboolean(L, res);
  return 1;
}