local exports = {}

local function getAddress(aob, errorMsg, modifierFunc)
  local address = core.AOBScan(aob, 0x400000)
  if address == nil then
    log(ERROR, errorMsg)
    error("'gmResourceModifier' can not be initialized.")
  end
  if modifierFunc == nil then
    return address;
  end
  return modifierFunc(address)
end

--[[ Main Func ]]--

exports.enable = function(self, moduleConfig, globalConfig)

  --[[ get addresses ]]--

  local placeToReplaceLoadGmsAddr = getAddress(
    "e8 ? ? ? ff b9 ? ? ? 02 e8 ? ? ? ff 53 6a 03",
    "'gmResourceModifier' was unable to find the gms loading call."
  )
  
  local actualLoadGmsAddress = getAddress(
    "53 55 8b 6c 24 0c 56 57 8b d9",
    "'gmResourceModifier' was unable to find the actual gms loading address."
  )
  
  local transformRGB555ToRGB565 = getAddress(
    "55 8b ec 83 ec 0c 8b 45 08",
    "'gmResourceModifier' was unable to find the RGB555 To RGB565 func address."
  )

  local gmImageHeaderAddr = getAddress(
    "c1 e0 04 81 c1 ? ? ? 00 50 51",
    "'gmResourceModifier' was unable to find the image header address.",
    function(foundAddress) return core.readInteger(foundAddress + 5) end
  )

  local gmSizesAddr = getAddress(
    "89 14 bd ? ? ? 00 eb 53",
    "'gmResourceModifier' was unable to find the gm sizes address.",
    function(foundAddress) return core.readInteger(foundAddress + 3) end
  )

  local gmOffsetAddr = getAddress(
    "8b 2c bd ? ? ? 00 03 d7 3b fa",
    "'gmResourceModifier' was unable to find the gm offset address.",
    function(foundAddress) return core.readInteger(foundAddress + 3) end
  )
  
  local pixelFormatAddr = getAddress(
    "81 3d ? ? ? 00 65 05 00 00 75 40",
    "'gmResourceModifier' was unable to find the address of the pixel format.",
    function(foundAddress) return core.readInteger(foundAddress + 2) end
  )


  --[[ load module ]]--
  
  local requireTable = require("gmResourceModifier.dll") -- loads the dll in memory and runs luaopen_gmResourceModifier
  
  -- no wrapping needed?
  self.LoadGm1Resource = function(self, ...) return requireTable.lua_LoadGm1Resource end
  self.FreeGm1Resource = function(self, ...) return requireTable.lua_FreeGm1Resource end
  self.SetGm = function(self, ...) return requireTable.lua_SetGm end
  self.LoadResourceFromImage = function(self, ...) return requireTable.lua_LoadResourceFromImage end
  

  --[[ modify code ]]--
  
  -- write the call to the own function
  core.writeCode(
    placeToReplaceLoadGmsAddr,
    {0xE8, requireTable.funcAddress_DetouredLoadGmFiles - placeToReplaceLoadGmsAddr - 5}  -- call to func
  )
  
  -- give actual load gms function
  core.writeCode(
    requireTable.address_ActualLoadGmsFunc,
    {actualLoadGmsAddress}
  )
  
  -- give actual RGB transform function
  core.writeCode(
    requireTable.address_TransformTgxFromRGB555ToRGB565,
    {transformRGB555ToRGB565}
  )
  
  -- give address of image header
  core.writeCode(
    requireTable.address_ShcImageHeaderStart,
    {gmImageHeaderAddr}
  )
  
  -- give address of image sizes
  core.writeCode(
    requireTable.address_ShcSizesStart,
    {gmSizesAddr}
  )
  
  -- give address of image offset
  core.writeCode(
    requireTable.address_ShcOffsetStart,
    {gmOffsetAddr}
  )
  
  -- give address to pixel format
  core.writeCode(
    requireTable.address_GamePixelFormatAddr,
    {pixelFormatAddr}
  )

  
  --[[ use config ]]--
  
  -- none at the moment


  --[[ test code ]]--
  
  --local resId = self.LoadGm1Resource("ucp/resources/tile_castle.gm1")
  --self.SetGm(0x34, -1, resId, -1)
  
  --local resId = self.LoadGm1Resource("gm/tile_castle.gm1")
  --self.SetGm(0x34, -1, resId, -1)
  
  --local resId2 = self.LoadGm1Resource("gm/anim_windmill.gm1")
  --self.SetGm(0x1a, -1, resId2, -1)

end

exports.disable = function(self, moduleConfig, globalConfig) error("not implemented") end

return exports