#include "rendering_server.hpp"
#include <system_error>

#include "common/asset_types.hpp"
#include "source/common/logging/logger.hpp"
#include "source/rendering/common/rendering_type.hpp"

namespace gravity {

auto RenderingServer::initialize() -> boost::asio::awaitable<std::error_code> {
  co_return co_await assets_.initialize();
}

auto RenderingServer::draw() -> boost::asio::awaitable<void> {

  co_await loadAsset(1);
  co_await loadAsset(2);

  co_return;
}

auto RenderingServer::loadAsset(AssetId asset_id) -> boost::asio::awaitable<std::error_code> {

  auto asset{ assets_.getAsset(asset_id) };

  if (!asset.has_value()) {
    LOG_ERROR("asset not found");
    co_return asset.error();
  }

  switch (asset.value()->type) {
    case AssetType::Unknown:
      co_return Error::UnimplementedError;
    case AssetType::Shader:
      co_spawn(
          strands_.getStrand(StrandLanes::Main),
          loadShaderModule(std::get<ShaderAssetDescriptor>(asset.value()->data)),
          boost::asio::detached);
      co_spawn(
          strands_.getStrand(StrandLanes::Main),
          loadShaderModule(std::get<ShaderAssetDescriptor>(asset.value()->data)),
          boost::asio::detached);
      co_spawn(
          strands_.getStrand(StrandLanes::Main),
          loadShaderModule(std::get<ShaderAssetDescriptor>(asset.value()->data)),
          boost::asio::detached);
      co_spawn(
          strands_.getStrand(StrandLanes::Main),
          loadShaderModule(std::get<ShaderAssetDescriptor>(asset.value()->data)),
          boost::asio::detached);
      co_spawn(
          strands_.getStrand(StrandLanes::Main),
          loadShaderModule(std::get<ShaderAssetDescriptor>(asset.value()->data)),
          boost::asio::detached);
      co_return Error::OK;
    case AssetType::Texture:
      co_return Error::UnimplementedError;
    case AssetType::Mesh:
      co_return Error::UnimplementedError;
    case AssetType::Material:
      co_return Error::UnimplementedError;
  }
}

auto RenderingServer::loadShaderModule(ShaderAssetDescriptor shader_asset_descriptor)
    -> boost::asio::awaitable<void> {

  if (shader_asset_descriptor.stages.contains(ShaderStage::Vertex)) {
    const auto& shader_stage_descriptor = shader_asset_descriptor.stages.at(ShaderStage::Vertex);

    ShaderResourceDescriptor shader_resource_description{ .path =
                                                              shader_stage_descriptor.spirvPath };

    auto vertHandle = co_await resources_.acquireShaderResource(shader_resource_description);
    if (!vertHandle) {
      LOG_ERROR("failed to load shader resource");
      co_return;
    }

    const auto& vert = resources_.getShader(vertHandle.value());

    ShaderDescriptor descriptor{
      .stage_ = ShaderStage::Vertex,
      .spirv_ = vert.spirv_,
      .hash_ = vert.hash_,
    };

    co_await device_.createShader(descriptor);

    co_await resources_.releaseShaderResource(vertHandle.value());
  }

  if (shader_asset_descriptor.stages.contains(ShaderStage::Fragment)) {
    const auto& shader_stage_descriptor = shader_asset_descriptor.stages.at(ShaderStage::Fragment);

    ShaderResourceDescriptor shader_resource_description{ .path =
                                                              shader_stage_descriptor.spirvPath };

    auto shader_resource_handle_expect =
        co_await resources_.acquireShaderResource(shader_resource_description);
    if (!shader_resource_handle_expect) {
      LOG_ERROR("failed to load shader resource");
      co_return;
    }

    auto shader_resource_handle = shader_resource_handle_expect.value();

    const auto& shader_resource = resources_.getShader(shader_resource_handle);

    ShaderDescriptor descriptor{
      .stage_ = ShaderStage::Fragment,
      .spirv_ = shader_resource.spirv_,
      .hash_ = shader_resource.hash_,
    };

    co_await device_.createShader(descriptor);

    co_await resources_.releaseShaderResource(shader_resource_handle);
  }

  // resources_.releaseShaderResource(fragHandle);
}

}  // namespace gravity