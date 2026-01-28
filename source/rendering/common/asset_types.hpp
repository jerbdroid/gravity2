#pragma once

#include "source/rendering/common/rendering_type.hpp"

#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

namespace gravity {

using AssetId = int64_t;

enum class AssetType : uint8_t { Unknown, Shader, Texture, Mesh, Material };

inline auto assetTypeFromString(std::string_view str) -> AssetType {
  if (str == "shader") {
    return AssetType::Shader;
  }
  if (str == "texture") {
    return AssetType::Texture;
  }
  if (str == "mesh") {
    return AssetType::Mesh;
  }
  if (str == "material") {
    return AssetType::Material;
  }

  return AssetType::Unknown;
}

struct ShaderStageDescriptor {
  std::string spirvPath;
  std::string metaPath;
};

struct ShaderAssetDescriptor {
  std::unordered_map<ShaderStage, ShaderStageDescriptor> stages;
};

struct TextureAssetDescriptor {
  std::string imagePath;
};

struct AssetDescriptor {
  AssetType type;
  std::variant<ShaderAssetDescriptor, TextureAssetDescriptor> data;
};

}  // namespace gravity