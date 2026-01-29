include "./premake/dependencies.lua"


function addLibrariesFromDirA(dir)
	local files = os.matchfiles(path.join(dir, "*.lib"))
	local libs = {}
	for _, file in ipairs(files) do
		table.insert(libs, path.getbasename(file))
	end
	libdirs { dir }
	links(libs)
end

function addLibrariesFromDirB(dir)
	-- Recursively get all .lib files from the specified directory and subdirectories
	local files = os.matchfiles(path.join(dir, "**", "*.lib"))

	for _, file in ipairs(files) do
		-- Extract the base name without extension for linking
		local libName = path.getbasename(file)

		-- Add the full path of the library's directory to the libdirs
		local libDir = path.getdirectory(file)
		libdirs { libDir }

		-- Link the library by its base name
		links { libName }
	end
end

workspace "Gateway"
architecture "x86_64"
startproject "Gravity"

configurations
{
	"Debug",
	"Release"
}


multiprocessorcompile("on")

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"



group "Dependencies"
include "modules/glfw"
-- include "modules/lua"
group ""

group "Core"
project "Gravity"
kind "ConsoleApp"
language "C++"
cppdialect "C++latest"
staticruntime "on"
exceptionhandling "Off"

targetdir("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
objdir("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")

files
{
	"source/**.hpp",
	"source/**.cpp",
	"source/**.h",
}


removefiles {
}


defines
{
	"SPDLOG_NO_EXCEPTIONS",
	"VULKAN_HPP_NO_EXCEPTIONS",
	"BASE64_STATIC_DEFINE",
	"BOOST_NO_EXCEPTIONS",
	"BOOST_ALL_NO_LIB", 
	"NOMINMAX",
	"BOOST_JSON_NO_LIB",
	"WIN32_LEAN_AND_MEAN",
}

includedirs
{
	"./",
	"%{IncludeDirectory.gsl}",
	"%{IncludeDirectory.entt}",
	"%{IncludeDirectory.spdlog}",
	"%{IncludeDirectory.magic_enum}", 
	"%{IncludeDirectory.glad}",
	"%{IncludeDirectory.glm}",  
	"%{IncludeDirectory.glfw}",  
	"%{IncludeDirectory.vulkan_sdk}", 
}

links
{
	"GLFW", 
	"%{Library.vulkan}",
}

vpaths {
    ["Source"] = "**.cpp",
    ["Headers"] = "**.h"
}
 
filter "system:windows"
systemversion "latest"

links { "ws2_32" }

filter "configurations:Debug"

includedirs
{
	"%{IncludeDirectory.boostdbg}",
	"%{IncludeDirectory.absldbg}", 
}
addLibrariesFromDirA("modules/abseil-cpp/build/install/debug/lib")
addLibrariesFromDirA("modules/boost/build/install/debug/lib")
defines { "DEBUG" }
runtime "Debug"
symbols "on"

filter "configurations:Release"
includedirs
{
	"%{IncludeDirectory.boost}", 
	"%{IncludeDirectory.absl}", 
}
addLibrariesFromDirA("modules/abseil-cpp/build/install/release/lib")
addLibrariesFromDirA("modules/boost/build/install/release/lib") 
defines { "NDEBUG" }
runtime "Release"
optimize "on"

filter { "not action:vs*" }
-- Set the tools explicitly
toolset "clang"
group ""

filter { "action:vs*" }
buildoptions { "/utf-8", "/bigobj" }
