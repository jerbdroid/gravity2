#pragma once

#include "asset_manager.hpp"
#include "source/common/scheduler/scheduler.hpp"
#include "source/rendering/asset_manager.hpp"
#include "source/rendering/device/rendering_device.hpp"
#include "source/rendering/resource_manager.hpp"

#include "magic_enum.hpp"

#include <cstdint>

namespace gravity {

struct MaterialDescription {
  std::string vertex_shader_path_;
  std::string fragment_shader_path_;
};

constexpr size_t ShaderStageCount = magic_enum::enum_count<ShaderStage>();

struct ShaderGpuResource {
  std::array<ShaderHandle, ShaderStageCount> stages_;
  std::bitset<ShaderStageCount> present_;
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

  std::unordered_map<AssetId, ShaderGpuResource> shader_cache_;

  auto loadShaderModules(const ShaderAssetDescriptor& shader_asset_descriptor)
      -> boost::asio::awaitable<std::expected<ShaderGpuResource, std::error_code>>;
  auto loadShaderStage(ShaderStage stage, const ShaderStageDescriptor& shader_stage_descriptor)
      -> boost::asio::awaitable<std::expected<ShaderHandle, std::error_code>>;
};

}  // namespace gravity