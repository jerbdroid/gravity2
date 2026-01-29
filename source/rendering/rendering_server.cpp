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
    case AssetType::Shader: {
      auto shader_resource_expect = co_await co_spawn(
          strands_.getStrand(StrandLanes::Main),
          loadShader(std::get<ShaderDescriptor>(asset.value()->data_)), boost::asio::use_awaitable);

      if (!shader_resource_expect) {
        LOG_ERROR("failed to load shader resource");
        co_return shader_resource_expect.error();
      }

      shader_resource_cache_.emplace(asset_id, std::move(shader_resource_expect.value()));
      co_return Error::OK;
    }
    case AssetType::Texture:
      co_return Error::UnimplementedError;
    case AssetType::Mesh:
      co_return Error::UnimplementedError;

    case AssetType::Material: {
      auto material_resource_expect = co_await co_spawn(
          strands_.getStrand(StrandLanes::Main),
          loadMaterial(std::get<MaterialDescriptor>(asset.value()->data_)),
          boost::asio::use_awaitable);

      if (!material_resource_expect) {
        LOG_ERROR("failed to load material resource");
        co_return material_resource_expect.error();
      }

      material_resource_cache_.emplace(asset_id, std::move(material_resource_expect.value()));
      co_return Error::OK;
    }
  }
}

auto RenderingServer::loadShader(const ShaderDescriptor& shader_descriptor)
    -> boost::asio::awaitable<std::expected<ShaderResource, std::error_code>> {

  ShaderResource shader_resource{};

  for (auto shader_stage : magic_enum::enum_values<ShaderStage>()) {
    if (!shader_descriptor.stages_.contains(shader_stage)) {
      LOG_TRACE("stage assets not found skipping; stage: {}", magic_enum::enum_name(shader_stage));
      continue;
    }
    const auto& stage_descriptor = shader_descriptor.stages_.at(shader_stage);

    auto shader_handle_expect = co_await loadShaderStage(shader_stage, stage_descriptor);
    if (!shader_handle_expect) {
      for (auto index = 0; index < shader_resource.stages_.size(); ++index) {
        if (shader_resource.present_.test(index)) {
          co_await device_.destroyShaderModule(std::move(shader_resource.stages_[index]));
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
    -> boost::asio::awaitable<std::expected<ShaderModuleHandle, std::error_code>> {

  ShaderSourceResourceDescriptor resource_descriptor{ .path = shader_stage_descriptor.spirv_path_ };

  auto handle_expect = co_await resources_.acquireShaderSourceResource(resource_descriptor);
  if (!handle_expect) {
    LOG_ERROR(
        "failed to load shader resource; stage: {}, spirv_path: {}", magic_enum::enum_name(stage),
        resource_descriptor.path);
    co_return std::unexpected(Error::InternalError);
  }

  auto handle = handle_expect.value();
  const auto& shader = resources_.getShader(handle);

  ShaderModuleDescriptor descriptor{
    .stage_ = stage,
    .spirv_ = shader.spirv_,
    .hash_ = shader.hash_,
  };

  auto shader_handle{ co_await device_.createShaderModule(descriptor) };
  co_await resources_.releaseShaderSourceResource(handle);
  co_return shader_handle;
}

auto RenderingServer::loadMaterial(const MaterialDescriptor& material_descriptor)
    -> boost::asio::awaitable<std::expected<MaterialResource, std::error_code>> {
  co_return std::unexpected(Error::UnimplementedError);
}

}  // namespace gravity