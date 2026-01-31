#pragma once

#include "common/asset_types.hpp"
#include "source/rendering/common/asset_types.hpp"

#include "boost/asio/awaitable.hpp"
#include "boost/json/object.hpp"

#include <cstdint>
#include <expected>
#include <span>
#include <system_error>
#include <unordered_map>

namespace gravity {

enum class ExpectedTypes : uint8_t { String, Integer, Boolean, List };

struct RequiredParameters {
  const char* name_;
  ExpectedTypes expected_type_;
};

class AssetManager {
 public:
  auto initialize() -> boost::asio::awaitable<std::error_code>;

  [[nodiscard]] auto getAsset(AssetId asset_id) const
      -> std::expected<const AssetDescriptor*, std::error_code>;

 private:
  std::unordered_map<AssetId, AssetDescriptor> assets_;

  static auto validateRequiredParameters(
      const boost::json::object& object, std::span<const RequiredParameters> parameters)
      -> std::error_code;

  static auto parseShaderDescriptor(const boost::json::object& asset)
      -> std::expected<ShaderDescriptor, std::error_code>;

  static auto parseMaterialDescriptor(const boost::json::object& asset)
      -> std::expected<MaterialDescriptor, std::error_code>;

  static auto parseTextureDescriptor(const boost::json::object& asset)
      -> std::expected<TextureDescriptor, std::error_code>;

  static auto parseMeshDescriptor(const boost::json::object& asset)
      -> std::expected<MeshDescriptor, std::error_code>;
};

}  // namespace gravity