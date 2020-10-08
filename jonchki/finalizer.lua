local t = ...

local atInstall = {
  -- Copy the complete "lib" folder to the "lib" folder in the installation base.
  ['${depack_path_org.muhkuh.tools.flasher_pt.flasher}/lib'] = '${install_base}/lib'
}
for strSrc, strDst in pairs(atInstall) do
  t:install(strSrc, strDst)
end

return true
