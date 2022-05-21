project "WalnutApp"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++17"
   targetdir "bin/%{cfg.buildcfg}"
   staticruntime "off"

   files { "src/**.h", "src/**.cpp", "../vendor/ImGuiFileDialog/**.cpp" }

   includedirs
   {
      "../vendor",
      "../vendor/imgui",
      "../vendor/glfw/include",
      "../vendor/json",
      "../vendor/libp2p",
      "../vendor/miniz",
      "../vendor/ImGuiFileDialog",

      "../Walnut/src",

      "%{IncludeDir.vapoursynth}",
      "%{IncludeDir.VulkanSDK}",
      "%{IncludeDir.glm}",
   }

    links
    {
        "Walnut"
    }
    if string.find(_ACTION, "gmake") then
       -- Premake5 is about to generate gmake or gmake2 build Makefiles, and
       -- Makefile support is still new and it does not generate "links" for
       -- the dependencies needed by Walnut. Add the Linux libs in this case:
       if os.istarget("linux") then
          links { "imgui", "glfw", "vulkan", "libp2p", "miniz", "vapoursynth", "vapoursynth-script" }
       end
    end

   targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
   objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

   filter "system:windows"
      systemversion "latest"
      defines { "WL_PLATFORM_WINDOWS" }

   filter "configurations:Debug"
      defines { "WL_DEBUG" }
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      defines { "WL_RELEASE" }
      runtime "Release"
      optimize "On"
      symbols "On"

   filter "configurations:Dist"
      kind "WindowedApp"
      defines { "WL_DIST" }
      runtime "Release"
      optimize "On"
      symbols "Off"
