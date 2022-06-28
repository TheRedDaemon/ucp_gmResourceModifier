#pragma once

#include <memory>
#include <unordered_map>

#include "gmResourceModifierHeader.h"

namespace GRMH = GmResourceModifierHeader; // easier to use

/* classes, structs and enums */

// source: https://github.com/PodeCaradox/Gm1KonverterCrossPlatform/blob/master/Gm1KonverterCrossPlatform/Files/GM1FileHeader.cs
enum class Gm1Type : int
{
  INTERFACE         = 1,  // Interface items and some building animations. Images are stored similar to TGX images.
  ANIMATIONS        = 2,  // Animations
  TILES_OBJECT      = 3,  // Buildings. Images are stored similar to TGX images but with a Tile object.
  FONT              = 4,  // Font. TGX format.
  NO_COMPRESSION_1  = 5,
  TGX_CONST_SIZE    = 6,
  NO_COMPRESSION_2  = 7
};

// source: https://github.com/PodeCaradox/Gm1KonverterCrossPlatform/blob/master/Gm1KonverterCrossPlatform/Files/GM1FileHeader.cs
struct Gm1Header
{
  unsigned int unknownField1;
  unsigned int unknownField2;
  unsigned int unknownField3;
  size_t numberOfPicturesInFile;
  unsigned int unknownField5;
  Gm1Type gm1Type;
  unsigned int unknownField7;
  unsigned int unknownField8;
  unsigned int unknownField9;
  unsigned int unknownField10;
  unsigned int unknownField11;
  unsigned int unknownField12;
  size_t width;
  size_t height;
  unsigned int unknownField15;
  unsigned int unknownField16;
  unsigned int unknownField17;
  unsigned int unknownField18;
  size_t originX;
  size_t originY;
  size_t dataSize;
  unsigned int unknownField22;
  unsigned short colorPalette[10][256];
};


// source: https://github.com/PodeCaradox/Gm1KonverterCrossPlatform/blob/master/Gm1KonverterCrossPlatform/Files/TGXImageHeader.cs
struct ImageHeader
{
  unsigned short width;
  unsigned short height;
  unsigned short offsetX;
  unsigned short offsetY;
  unsigned short relativeDataPos;  // seems to be used to point to data to use instead
  unsigned short tileOffset;
  unsigned char direction;
  unsigned char horizontalOffsetOfImage;
  unsigned char buildingWidth;
  unsigned char animatedColor; // if alpha 1
};


struct Gm1Resource
{
  int resourceId{};
  bool done{ false };
  size_t refCounter{ 0 };

  std::unique_ptr<Gm1Header> gm1Header{};
  std::vector<ImageHeader> imageHeader{};
  std::vector<int> imageSizes{};
  std::vector<int> imageOffset{};
  std::vector<unsigned char> imageData{};
};


// singleton
class Gm1ResourceManager
{
private:
  inline static int idGiver{ 0 };
  inline static std::vector<int> freeIds{};

  inline static std::unordered_map<int, Gm1Resource> resources{};

public:

  static int CreateGm1Resource(const char* filename);
  static bool FreeGm1Resource(int resourceId);

  static void ReadyAllResources();  // for init
  static Gm1Resource* GetResource(int resourceId);

private:

  Gm1ResourceManager() = delete;

  static int GetId();
  static void ReturnId(int id);

  static bool ReadyResource(Gm1Resource& resource);

  // the compiler should optimize this
  static void LogHelper(LuaLog::LogLevel level, const char* start, const char* filename, const char* error);
};


class Replacer
{
public:
  const int gmIndex;
  const int firstImageIndex;
  Gm1Resource* mainReplacingResource{};
  std::unique_ptr<Gm1Resource> origResource{};

  // image index -> pair(resourceId, imageIndex)
  std::unordered_map<int, std::pair<int, int>> imageReplaced{};

  Replacer(int gmIndex, int firstImageIndex) : gmIndex(gmIndex), firstImageIndex(firstImageIndex){};
  Replacer() = delete;

  void readyOrigResource();

  // important -> no tests for -1 values -> given stuff needs to be tested before
  void copyToShc(Gm1Resource& resource, int imageIndex, int resourceImageIndex);
};


struct ColorAdapter
{
  enum class PixelFormat
  {
    NONE = 0x0,
    ARGB_1555 = 0x555,
    RGB_565 = 0x565,
  };

  enum class TransformType
  {
    NORMAL = 0,
    UNCOMPRESSED_2 = 1,
    COLOR_PALETTE = 2,
  };

  using ActualLoadGmsFunc = void (ColorAdapter::*)(char* fileNameArray);
  using TransformTgxFromRGB555ToRGB565 = void (ColorAdapter::*)(void* tgxDataPtr, int tgxByteSize);

  inline static PixelFormat* gamePixelFormat{};

  // actually not color adapter
  inline static ColorAdapter* shcTransformStruct{ nullptr };
  inline static ActualLoadGmsFunc actualLoadGmsFunc{ nullptr };
  inline static TransformTgxFromRGB555ToRGB565 transformTgxFromRGB555ToRGB565{ nullptr };

  // funcs

  // used for simple uncompressed transform
  static void TransformRGB555ToRGB565(unsigned short* data, size_t byteSize, TransformType transformType);

  static void AdaptGm1Resource(Gm1Resource& resource);

  // other funcs
  void __thiscall detouredLoadGmFiles(char* fileNameArray);
};


/* variables */

inline ImageHeader* shcImageHeaderStart{};
inline int* shcSizesStart{};
inline int* shcOffsetStart{};


/* exports */

extern "C" __declspec(dllexport) int __stdcall LoadGm1Resource(const char* filepath);
extern "C" __declspec(dllexport) bool __stdcall SetGm(int gmID, int imageInGm, int resourceId, int imageInResource);
extern "C" __declspec(dllexport) bool __stdcall FreeGm1Resource(int resourceId);

extern "C" __declspec(dllexport) int __stdcall LoadImageAsInterfaceResource(const char* filepath);

/* LUA */

extern "C" __declspec(dllexport) int __cdecl lua_LoadGm1Resource(lua_State * L);
extern "C" __declspec(dllexport) int __cdecl lua_SetGm(lua_State * L);
extern "C" __declspec(dllexport) int __cdecl lua_FreeGm1Resource(lua_State * L);

extern "C" __declspec(dllexport) int __cdecl lua_LoadImageAsInterfaceResource(lua_State * L);