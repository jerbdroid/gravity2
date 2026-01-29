#include "rendering_server.hpp"

#include "common/asset_types.hpp"
#include "source/common/error.hpp"
#include "source/common/logging/logger.hpp"
#include "source/rendering/common/rendering_type.hpp"

#include "boost/asio/awaitable.hpp"
#include "boost/asio/use_awaitable.hpp"
#include "gsl/gsl"

#include <expected>
#include <system_error>

namespace gravity {

auto RenderingServer::initialize() -> boost::asio::awaitable<std::error_code> {
  co_return co_await assets_.initialize();
}

auto RenderingServer::draw() -> boost::asio::awaitable<void> {

  co_await loadAsset(1);
  co_await loadAsset(1);
  co_await loadAsset(1);
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
    case AssetType::Shader: {
      auto shader_gpu_resource_expect = co_await co_spawn(
          strands_.getStrand(StrandLanes::Main),
          loadShaderModules(std::get<ShaderAssetDescriptor>(asset.value()->data)),
          boost::asio::use_awaitable);
      if (!shader_gpu_resource_expect) {
        LOG_ERROR("failed to load shader resource");
        co_return shader_gpu_resource_expect.error();
      }
      shader_cache_.emplace(asset_id, std::move(shader_gpu_resource_expect.value()));
      co_return Error::OK;
    }
    case AssetType::Texture:
      co_return Error::UnimplementedError;
    case AssetType::Mesh:
      co_return Error::UnimplementedError;
    case AssetType::Material:
      co_return Error::UnimplementedError;
  }
}

auto RenderingServer::loadShaderModules(const ShaderAssetDescriptor& shader_asset_descriptor)
    -> boost::asio::awaitable<std::expected<ShaderGpuResource, std::error_code>> {

  ShaderGpuResource shader_resource{};

  for (auto shader_stage : magic_enum::enum_values<ShaderStage>()) {
    if (!shader_asset_descriptor.stages.contains(shader_stage)) {
      LOG_TRACE("stage assets not found skipping; stage: {}", magic_enum::enum_name(shader_stage));
      continue;
    }
    const auto& stage_descriptor = shader_asset_descriptor.stages.at(shader_stage);

    auto shader_handle_expect = co_await loadShaderStage(shader_stage, stage_descriptor);
    if (!shader_handle_expect) {
      for (auto index = 0; index < shader_resource.stages_.size(); ++index) {
        if (shader_resource.present_.test(index)) {
          co_await device_.destroyShader(std::move(shader_resource.stages_[index]));
        }
      }
      shader_resource.present_.reset();

      LOG_ERROR("failed to load shader; stage: {}", magic_enum::enum_name(shader_stage));
      co_return std::unexpected(shader_handle_expect.error());
    }

    auto index_opt = magic_enum::enum_index(shader_stage);
    if (!index_opt) {
      LOG_ERROR("Invalid shader stage enum value {}", magic_enum::enum_name(shader_stage));
      co_return std::unexpected(make_error_code(Error::NotFoundError));
    }
    const auto index = *index_opt;

    shader_resource.stages_[index] = shader_handle_expect.value();
    shader_resource.present_.set(index);
  }

  co_return shader_resource;
}

auto RenderingServer::loadShaderStage(
    ShaderStage stage, const ShaderStageDescriptor& shader_stage_descriptor)
    -> boost::asio::awaitable<std::expected<ShaderHandle, std::error_code>> {

  ShaderResourceDescriptor resource_descriptor{ .path = shader_stage_descriptor.spirvPath };

  auto handle_expect = co_await resources_.acquireShaderResource(resource_descriptor);
  if (!handle_expect) {
    LOG_ERROR(
        "failed to load shader resource; stage: {}, spirv_path: {}", magic_enum::enum_name(stage),
        resource_descriptor.path);
    co_return std::unexpected(Error::InternalError);
  }

  auto handle = handle_expect.value();
  const auto& shader = resources_.getShader(handle);

  ShaderDescriptor descriptor{
    .stage_ = stage,
    .spirv_ = shader.spirv_,
    .hash_ = shader.hash_,
  };

  auto shader_handle{ co_await device_.createShader(descriptor) };
  co_await resources_.releaseShaderResource(handle);
  co_return shader_handle;
}

}  // namespace gravity