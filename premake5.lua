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
	-- "modules/imgui/*.cpp",
	-- "modules/imgui/*.h",
	-- "modules/crypto-algorithms/*.hpp",
	-- "modules/crypto-algorithms/*.cpp",
	-- "modules/imgui/backends/imgui_impl_glfw.*",
	-- "modules/spirv_reflect/spirv_reflect.h",
	-- "modules/spirv_reflect/spirv_reflect.c",
	-- "modules/glslang/glslang/**/*.cpp",
	-- "modules/glslang/glslang/**/*.h",
	-- "modules/glslang/SPIRV/**/*.cpp",
	-- "modules/glslang/SPIRV/**/*.h",
	-- "modules/glslang/SPIRV/*.cpp",
	-- "modules/glslang/SPIRV/*.h",
	-- "generated/**"

}


removefiles {
	-- "source/rendering/composition/**",
	-- "modules/glslang/glslang/HLSL/*.cpp",
	-- "modules/glslang/glslang/HLSL/*.h",
	-- "modules/glslang/glslang/OSDependent/**/*.cpp",
	-- "modules/glslang/glslang/OSDependent/**/*.h",
	-- "modules/crypto-algorithms/*test.c"
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
	-- "%{IncludeDirectory.boost_asio}",  
	-- "%{IncludeDirectory.boost_align}",
	-- "%{IncludeDirectory.boost_config}",  
	-- "%{IncludeDirectory.boost_core}",  
	-- "%{IncludeDirectory.boost_assert}",  
	-- "%{IncludeDirectory.boost_throw_exception}",  
	-- "%{IncludeDirectory.boost_system}",  
	-- "%{IncludeDirectory.boost_static_assert}",  
	-- "%{IncludeDirectory.boost_context}",  
	-- "%{IncludeDirectory.boost_date_time}",  
	-- "%{IncludeDirectory.boost_json}",
	-- "%{IncludeDirectory.boost_container}",
	-- "%{IncludeDirectory.boost_intrusive}",
	-- "%{IncludeDirectory.boost_move}",
	-- "%{IncludeDirectory.boost_container_hash}",
	-- "%{IncludeDirectory.boost_describe}",
	-- "%{IncludeDirectory.boost_mp11}",
	-- "%{IncludeDirectory.boost_endian}",
	-- "%{IncludeDirectory.boost_winapi}",
	-- "%{IncludeDirectory.boost_predef}", 
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
