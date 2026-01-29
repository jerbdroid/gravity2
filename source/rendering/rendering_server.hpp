#pragma once

#include "asset_manager.hpp"
#include "source/common/scheduler/scheduler.hpp"
#include "source/rendering/asset_manager.hpp"
#include "source/rendering/device/rendering_device.hpp"
#include "source/rendering/resource_manager.hpp"

#include <cstdint>

namespace gravity {

struct MaterialDescription {
  std::string vertex_shader_path_;
  std::string fragment_shader_path_;
};

class RenderingServer {
 public:
  enum class StrandLanes : uint8_t { Main, _Count };
  using StrandGroup = StrandGroup<RenderingServer>;

  ~RenderingServer() = default;

  RenderingServer(Scheduler& scheduler, RenderingDevice& device)
      : device_{ device },
        strands_{ scheduler.makeStrands<RenderingServer>() },
        resources_{ scheduler.makeStrands<ResourceManager>() } {}

  auto initialize() -> boost::asio::awaitable<std::error_code>;

  auto draw() -> boost::asio::awaitable<void>;

  auto loadAsset(AssetId asset_id) -> boost::asio::awaitable<std::error_code>;

 private:
  RenderingDevice& device_;
  StrandGroup strands_;
  AssetManager assets_;
  ResourceManager resources_;

  auto loadShaderModule(ShaderAssetDescriptor shader_asset_descriptor)
      -> boost::asio::awaitable<void>;
};

}  // namespace gravity