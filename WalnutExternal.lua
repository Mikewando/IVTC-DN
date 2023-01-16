-- WalnutExternal.lua

VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"
IncludeDir["glm"] = "../vendor/glm"
IncludeDir["vapoursynth"] = "../vendor/vapoursynth/include"

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibraryDir["vapoursynth"] = "../vendor/vapoursynth/lib64"

Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"


function linux_dpkg_check_if_package_is_installed(package)
   if os.isfile("/usr/bin/dpkg-query") then -- It's Debian or a derivative of it:
      local result, errorCode = os.outputof("/usr/bin/dpkg-query -Wf'${Status}' " .. package)
      if errorCode ~= 0 or result ~= "install ok installed" then
         premake.error("Install " .. package .. " using: sudo apt-get install " .. package)
      end
      premake.info(package .. " is installed")
   else -- Not a Debian-based Linux distro, for now just give a hint:
      premake.warn("Info: Please be sure that a package like " .. package .. " is installed")
    end
end


group "Dependencies"
   include "vendor/imgui"
   -- When build host and build target are Linux, see if we can check for needed libs:
   if os.ishost("linux") and os.istarget("linux") then
      linux_dpkg_check_if_package_is_installed("libglfw3-dev")
      linux_dpkg_check_if_package_is_installed("libvulkan-dev")
      linux_dpkg_check_if_package_is_installed("mesa-vulkan-drivers")
      linux_dpkg_check_if_package_is_installed("vulkan-tools")
   else
      -- For other operating systems, build GLFW in the cloned git submodule:
      include "vendor/GLFW"
   end
   include "vendor/miniz"
   include "vendor/libp2p"
group ""

group "Core"
include "Walnut"
group ""
