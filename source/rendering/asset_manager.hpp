#pragma once

#include "source/rendering/common/asset_types.hpp"

#include "boost/asio/awaitable.hpp"

#include <expected>
#include <system_error>
#include <unordered_map>

namespace gravity {

class AssetManager {
 public:
  auto initialize() -> boost::asio::awaitable<std::error_code>;

  [[nodiscard]] auto getAsset(AssetId asset_id) const
      -> std::expected<const AssetDescriptor*, std::error_code>;

 private:
  std::unordered_map<AssetId, AssetDescriptor> assets_;
};

}  // namespace gravity