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
#include <vector>

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

  auto loadAndCache = [&](auto&& load_coroutine,
                          auto& cache) -> boost::asio::awaitable<std::error_code> {
    if (cache.contains(asset_id)) {
      LOG_DEBUG("asset already loaded");
      co_return Error::OK;
    }

    auto resource = co_await co_spawn(
        strands_.getStrand(StrandLanes::Main), std::move(load_coroutine),
        boost::asio::use_awaitable);

    if (!resource) {
      LOG_ERROR("failed to load resource");
      co_return resource.error();
    }

    cache.emplace(asset_id, std::move(*resource));
    co_return Error::OK;
  };

  const auto& asset_descriptor = *asset.value();
  switch (asset_descriptor.type) {
    case AssetType::Shader:
      co_return co_await loadAndCache(
          loadShader(std::get<ShaderDescriptor>(asset_descriptor.data_)), shader_resource_cache_);
    case AssetType::Texture:
      co_return co_await loadAndCache(
          loadTexture(std::get<TextureDescriptor>(asset_descriptor.data_)),
          texture_resource_cache_);
    case AssetType::Mesh:
      co_return co_await loadAndCache(
          loadMesh(std::get<MeshDescriptor>(asset_descriptor.data_)), mesh_resource_cache_);
    case AssetType::Material:
      co_return co_await loadAndCache(
          loadMaterial(std::get<MaterialDescriptor>(asset_descriptor.data_)),
          material_resource_cache_);
    default:
      std::unreachable();
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
      co_return std::unexpected(make_error_code(Error::InternalError));
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

  ResourceDescriptor resource_descriptor{ .type_ = ResourceType::Shader,
                                          .path_ = shader_stage_descriptor.spirv_path_ };

  auto expect_lease = co_await resources_.acquireResource(resource_descriptor);
  if (!expect_lease) {
    LOG_ERROR(
        "failed to load shader resource; stage: {}, spirv_path: {}", magic_enum::enum_name(stage),
        resource_descriptor.path_);
    co_return std::unexpected(expect_lease.error());
  }

  const auto& shader = *(co_await resources_.getResource(expect_lease.value()));

  std::vector<uint32_t> spirv;
  spirv.resize(shader.data_.size() / sizeof(uint32_t));
  std::memcpy(spirv.data(), shader.data_.data(), shader.data_.size());

  ShaderModuleDescriptor descriptor{
    .stage_ = stage,
    .spirv_ = spirv,
    .hash_ = shader.hash_,
  };

  co_return co_await device_.createShaderModule(descriptor);
}

auto RenderingServer::loadMaterial(const MaterialDescriptor& material_descriptor)
    -> boost::asio::awaitable<std::expected<MaterialResource, std::error_code>> {
  co_return std::unexpected(Error::UnimplementedError);
}

auto RenderingServer::loadMesh(const MeshDescriptor& mesh_descriptor)
    -> boost::asio::awaitable<std::expected<MeshResource, std::error_code>> {
  co_return std::unexpected(Error::UnimplementedError);
}

auto RenderingServer::loadTexture(const TextureDescriptor& mesh_descriptor)
    -> boost::asio::awaitable<std::expected<TextureResource, std::error_code>> {

  // ShaderSourceResourceDescriptor resource_descriptor{ .path = shader_stage_descriptor.spirv_path_
  // };

  // auto handle_expect = co_await resources_.acquireShaderSourceResource(resource_descriptor);

  co_return std::unexpected(Error::UnimplementedError);
}

}  // namespace gravity