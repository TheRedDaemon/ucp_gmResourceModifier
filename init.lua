local exports = {}


--[[ Main Func ]]--

exports.enable = function(self, moduleConfig, globalConfig)

  --[[ get addresses ]]--

  local getTextFuncStart = core.AOBScan("8b 44 24 04 8d 50 fb", 0x400000)
  if getTextFuncStart == nil then
    print("'gmResourceModifier' was unable to find the start of the address that selects the right text strings.")
    error("'gmResourceModifier' can not be initialized.")
  end
  local backJmpAddress = getTextFuncStart + 7 -- address to jmp back to
  
  local callToLoadCRTexFunc = core.AOBScan("e8 ? ? ? ff e8 ? ? ? ff 6a 08", 0x400000)
  if callToLoadCRTexFunc == nil then
    print("'gmResourceModifier' was unable to find the call to load the CRTex file.")
    error("'gmResourceModifier' can not be initialized.")
  end
  
  local realLoadCRTexFunc = core.AOBScan("51 53 55 56 33 ed 8b f1 55 68", 0x400000)
  if realLoadCRTexFunc == nil then
    print("'gmResourceModifier' was unable to find the CRTex load func.")
    error("'gmResourceModifier' can not be initialized.")
  end

  --[[ load module ]]--
  
  local requireTable = require("gmResourceModifier.dll") -- loads the dll in memory and runs luaopen_gmResourceModifier
  
  for name, addr in pairs(requireTable.funcPtr) do
    self[name] = addr
  end
  
  -- no wrapping needed?
  self.SetText = requireTable.lua_SetText
  

  --[[ modify code ]]--
  
  -- write the jmp to the own function
  core.writeCode(
    getTextFuncStart,
    {0xE9, requireTable.funcAddress_GetMapText - getTextFuncStart - 5}  -- jmp to func
  )
  
  -- give return jump address to the dll
  core.writeCode(
    requireTable.address_ToNativeTextAddr,
    {backJmpAddress}
  )
  
  -- gives the address of crusaders CRTex load func
  core.writeCode(
    requireTable.address_RealLoadCRTFuncAddr,
    {realLoadCRTexFunc}
  )
  
  -- writes the call to the modified loadCRT in the mainLoop
  core.writeCode(
    callToLoadCRTexFunc,
    {0xE8, requireTable.funcAddress_InterceptedLoadCRTex - callToLoadCRTexFunc - 5}
  )
  
  
  --[[ use config ]]--
  
  -- none at the moment


  --[[ test code ]]--
  
  -- self.SetText(4, 0, "Güterchen")

end

exports.disable = function(self, moduleConfig, globalConfig) error("not implemented") end

return exports