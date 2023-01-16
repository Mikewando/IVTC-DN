-- premake5.lua
workspace "IVTCDN"
   architecture "x64"
   configurations { "Debug", "Release", "Dist" }
   startproject "IVTCDN"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

include "WalnutExternal.lua"
include "WalnutApp"