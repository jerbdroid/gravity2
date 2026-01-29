VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDirectory = {}
IncludeDirectory["stb"] = "%{wks.location}/modules/stb"
IncludeDirectory["glfw"] = "%{wks.location}/modules/glfw/include"
IncludeDirectory["glad"] = "%{wks.location}/modules/glad/include"
IncludeDirectory["glm"] = "%{wks.location}/modules/glm"
IncludeDirectory["entt"] = "%{wks.location}/modules/entt/src"
IncludeDirectory["spdlog"] = "%{wks.location}/modules/spdlog/include"
IncludeDirectory["fmtlib"] = "%{wks.location}/modules/fmt/include"
IncludeDirectory["magic_enum"] = "%{wks.location}/modules/magic_enum/include/magic_enum"
IncludeDirectory["vma"] = "%{wks.location}/modules/vma/include"
IncludeDirectory["vda"] = "%{wks.location}/modules/vda"
IncludeDirectory["sol2"] = "%{wks.location}/modules/sol2/include"
IncludeDirectory["assimp"] = "%{wks.location}/modules/assimp/include"
IncludeDirectory["spirvreflect"] = "%{wks.loaction}/modules/spirv_reflect"
IncludeDirectory["imgui"] = "%{wks.location}/modules/imgui"
IncludeDirectory["glslang"] = "%{wks.location}/modules/glslang"
IncludeDirectory["cryptoalgorithms"] = "%{wks.location}/modules/crypto-algorithms"
IncludeDirectory["base64"] = "%{wks.location}/modules/base64/include"
IncludeDirectory["yaml"] = "%{wks.location}/modules/yaml-cpp/include"
IncludeDirectory["flatbuffer"] = "%{wks.location}/modules/flatbuffers/include"
IncludeDirectory["libzmq"] = "%{wks.location}/modules/libzmq/include"
IncludeDirectory["readerwriterqueue"] = "%{wks.location}/modules/readerwriterqueue"
-- IncludeDirectory["lua"] = "%{wks.location}/modules/lua"
IncludeDirectory["protobuf"] = "%{wks.location}/modules/protobuf/src"
IncludeDirectory["ndf"] = "%{wks.location}/modules/nativefiledialog/src/include"
IncludeDirectory["taskflow"] = "%{wks.location}/modules/taskflow"
IncludeDirectory["gsl"] = "%{wks.location}/modules/GSL/include"
IncludeDirectory["vulkan_sdk"] = "%{VULKAN_SDK}/Include"

IncludeDirectory["absldbg"] = "%{wks.location}/modules/abseil-cpp/build/install/debug/include"
IncludeDirectory["absl"] = "%{wks.location}/modules/abseil-cpp/build/install/release/include"
IncludeDirectory["boostdbg"] = "%{wks.location}/modules/boost/build/install/debug/include/boost-1_90"
IncludeDirectory["boost"] = "%{wks.location}/modules/boost/build/install/release/include/boost-1_90"

IncludeDirectory["gtest"] = "%{wks.location}/modules/googletest/googletest/include"
IncludeDirectory["gmock"] = "%{wks.location}/modules/googletest/googlemock/include"



LibraryDirectory = {}
LibraryDirectory["glfw"] = "%{wks.location}/modules/glfw/lib"
LibraryDirectory["fmtlib"] = "%{wks.location}/modules/fmtlib/lib"
LibraryDirectory["vulkan_sdk"] = "%{VULKAN_SDK}/Lib"
LibraryDirectory["base64"] = "%{wks.location}/prebuilt/base64/lib"
LibraryDirectory["spirvreflect"] = "%{wks.location}/prebuilt/spriv-reflect/lib"
LibraryDirectory["protobuf"] = "%{wks.location}/prebuilt/protobuf"
LibraryDirectory["cryptoalgorithms"] = "%{wks.location}/prebuilt/cryptoalgorithms"
LibraryDirectory["glslang"] = "%{wks.location}/prebuilt/glslang"
LibraryDirectory["boost"] = "%{wks.location}/modules/boost/INSTALL/lib"


Library = {}
Library["glfw"] = "%{LibraryDirectory.glfw}/glfw3.lib"
Library["spdlog"] = "%{LibraryDirectory.spdlog}/spdlog.lib"
Library["fmtlib"] = "%{LibraryDirectory.fmtlib}/fmt.lib"
Library["vulkan"] = "%{LibraryDirectory.vulkan_sdk}/vulkan-1.lib"
Library["base64"] = "base64.lib"
Library["spirvreflect"] = "spirv-reflect.lib"
Library["protobuf"] = "protobuf.lib"
Library["cryptoalgorithms"] = "crypto.lib"
