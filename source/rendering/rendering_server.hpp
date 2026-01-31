#pragma once

#include "asset_manager.hpp"
#include "common/asset_types.hpp"
#include "source/common/scheduler/scheduler.hpp"
#include "source/rendering/asset_manager.hpp"
#include "source/rendering/device/rendering_device.hpp"
#include "source/rendering/resource_manager.hpp"

#include "magic_enum.hpp"

#include <cstdint>
#include <unordered_map>

namespace gravity {

constexpr size_t ShaderStageCount = magic_enum::enum_count<ShaderStage>();

struct ShaderResource {
  std::array<ShaderModuleHandle, ShaderStageCount> stages_;
  std::bitset<ShaderStageCount> present_;
};

struct MaterialDescription {};

struct MaterialResource {};

struct MeshResource {};

struct TextureResource {};

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

  std::unordered_map<AssetId, ShaderResource> shader_resource_cache_;
  std::unordered_map<AssetId, MaterialResource> material_resource_cache_;
  std::unordered_map<AssetId, MeshResource> mesh_resource_cache_;
  std::unordered_map<AssetId, TextureResource> texture_resource_cache_;

  auto loadShader(const ShaderDescriptor& shader_descriptor)
      -> boost::asio::awaitable<std::expected<ShaderResource, std::error_code>>;
  auto loadShaderStage(ShaderStage stage, const ShaderStageDescriptor& shader_stage_descriptor)
      -> boost::asio::awaitable<std::expected<ShaderModuleHandle, std::error_code>>;

  auto loadMaterial(const MaterialDescriptor& material_descriptor)
      -> boost::asio::awaitable<std::expected<MaterialResource, std::error_code>>;

  auto loadMesh(const MeshDescriptor& mesh_descriptor)
      -> boost::asio::awaitable<std::expected<MeshResource, std::error_code>>;

  auto loadTexture(const TextureDescriptor& mesh_descriptor)
      -> boost::asio::awaitable<std::expected<TextureResource, std::error_code>>;
};

}  // namespace gravity