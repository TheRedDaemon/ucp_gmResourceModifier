local exports = {}


--[[ Main Func ]]--

exports.enable = function(self, moduleConfig, globalConfig)

  --[[ get addresses ]]--

  local placeToReplaceLoadGmsAddr = core.AOBScan("e8 ? ? ? ff b9 ? ? ? 02 e8 ? ? ? ff 53 6a 03", 0x400000)
  if placeToReplaceLoadGmsAddr == nil then
    print("'gmResourceModifier' was unable to find the gms loading call.")
    error("'gmResourceModifier' can not be initialized.")
  end
  
  local actualLoadGmsAddress = core.AOBScan("53 55 8b 6c 24 0c 56 57 8b d9", 0x400000)
  if actualLoadGmsAddress == nil then
    print("'gmResourceModifier' was unable to find the actual gms loading address.")
    error("'gmResourceModifier' can not be initialized.")
  end
  
  local transformRGB555ToRGB565 = core.AOBScan("55 8b ec 83 ec 0c 8b 45 08", 0x400000)
  if transformRGB555ToRGB565 == nil then
    print("'gmResourceModifier' was unable to find the RGB555 To RGB565 func address.")
    error("'gmResourceModifier' can not be initialized.")
  end

  local gmImageHeaderAddr = core.AOBScan("c1 e0 04 81 c1 ? ? ? 00 50 51", 0x400000)
  if gmImageHeaderAddr == nil then
    print("'gmResourceModifier' was unable to find the image header address.")
    error("'gmResourceModifier' can not be initialized.")
  end
  gmImageHeaderAddr = core.readInteger(gmImageHeaderAddr + 5)

  local gmSizesAddr = core.AOBScan("89 14 bd ? ? ? 00 eb 53", 0x400000)
  if gmSizesAddr == nil then
    print("'gmResourceModifier' was unable to find the gm sizes address.")
    error("'gmResourceModifier' can not be initialized.")
  end
  gmSizesAddr = core.readInteger(gmSizesAddr + 3)

  local gmOffsetAddr = core.AOBScan("8b 2c bd ? ? ? 00 03 d7 3b fa", 0x400000)
  if gmOffsetAddr == nil then
    print("'gmResourceModifier' was unable to find the gm offset address.")
    error("'gmResourceModifier' can not be initialized.")
  end
  gmOffsetAddr = core.readInteger(gmOffsetAddr + 3)
  
  local pixelFormatAddr = core.AOBScan("81 3d ? ? ? 00 65 05 00 00 75 40", 0x400000)
  if pixelFormatAddr == nil then
    print("'gmResourceModifier' was unable to find the address of the pixel format.")
    error("'gmResourceModifier' can not be initialized.")
  end
  pixelFormatAddr = core.readInteger(pixelFormatAddr + 2)


  --[[ load module ]]--
  
  local requireTable = require("gmResourceModifier.dll") -- loads the dll in memory and runs luaopen_gmResourceModifier
  
  for name, addr in pairs(requireTable.funcPtr) do
    self[name] = addr
  end
  
  -- no wrapping needed?
  self.LoadGm1Resource = requireTable.lua_LoadGm1Resource
  self.FreeGm1Resource = requireTable.lua_FreeGm1Resource
  self.SetGm = requireTable.lua_SetGm
  self.LoadResourceFromImage = requireTable.lua_LoadResourceFromImage
  

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