#pragma once

#include "source/common/error.hpp"
#include "source/rendering/common/rendering_type.hpp"

#include <cassert>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <variant>

namespace gravity {

using AssetId = int64_t;

enum class AssetType : uint8_t { Shader, Texture, Mesh, Material };

inline auto assetTypeFromString(std::string_view str) -> std::expected<AssetType, std::error_code> {
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

  return std::unexpected(Error::SchemaError);
}

inline auto assetShaderStageFromString(std::string_view str)
    -> std::expected<ShaderStage, std::error_code> {
  if (str == "vertex") {
    return ShaderStage::Vertex;
  }
  if (str == "fragment") {
    return ShaderStage::Fragment;
  }

  return std::unexpected(Error::SchemaError);
}

struct ShaderStageDescriptor {
  std::string spirvPath;
  std::string metaPath;
};

struct ShaderDescriptor {
  std::unordered_map<ShaderStage, ShaderStageDescriptor> stages;
};

struct MaterialDescriptor {};

struct TextureDescriptor {
  std::string imagePath;
};

struct AssetDescriptor {
  AssetType type;
  std::variant<ShaderDescriptor, MaterialDescriptor, TextureDescriptor> data;
};

}  // namespace gravity