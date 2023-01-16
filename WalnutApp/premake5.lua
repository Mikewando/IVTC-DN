project "WalnutApp"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"
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
        "Walnut",
        "imgui",
        "glfw",
        "libp2p",
        "miniz",
    }
    if os.istarget("windows") then
      links { "%{LibraryDir.vapoursynth}/vapoursynth.lib", "%{LibraryDir.vapoursynth}/vsscript.lib" }
    end
    if os.istarget("linux") then
      links { "vapoursynth", "vapoursynth-script" }
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
