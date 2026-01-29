#include "asset_manager.hpp"

#include "common/asset_types.hpp"
#include "source/common/error.hpp"
#include "source/common/logging/logger.hpp"

#include "boost/asio.hpp"
#include "boost/json.hpp"

#undef GRAVITY_MODULE_NAME
#define GRAVITY_MODULE_NAME "resource_manager"

namespace json = boost::json;

namespace gravity {

auto AssetManager::initialize() -> boost::asio::awaitable<std::error_code> {
  LOG_TRACE("initializing asset manager");

  auto executor = co_await boost::asio::this_coro::executor;

  boost::asio::stream_file file{ executor, "resources/assetsdb.json",
                                 boost::asio::stream_file::read_only };
  boost::asio::streambuf dynamic_buffer{};

  auto [error_code, bytes] = co_await boost::asio::async_read(
      file, dynamic_buffer, boost::asio::transfer_all(),
      boost::asio::as_tuple(boost::asio::use_awaitable));

  if (error_code != boost::asio::error::eof && error_code) {
    LOG_ERROR("asset manager failed to load assets db; error: {}", error_code.message());
    co_return Error::InternalError;
  }

  std::istream input_stream(&dynamic_buffer);
  auto val = json::parse(input_stream);

  if (!val.is_array()) {
    LOG_ERROR("asset database is not valid");
    co_return Error::InternalError;
  }

  const auto& array = val.as_array();

  for (const auto& item : array) {
    if (item.is_object()) {
      const auto& asset = item.as_object();

      if (!asset.contains("id")) {
        LOG_ERROR("asset database is not valid; missing id field");
        co_return Error::InternalError;
      }

      auto [iterator, inserted] = assets_.emplace(asset.at("id").as_int64(), AssetDescriptor{});

      if (!asset.contains("type")) {
        LOG_ERROR("asset database is not valid; missing type field");
        co_return Error::InternalError;
      }

      iterator->second.type = assetTypeFromString(asset.at("type").as_string());

      if (iterator->second.type == AssetType::Shader) {
        if (!asset.contains("stages")) {
          LOG_ERROR("asset database is not valid; missing stages field");
          co_return Error::InternalError;
        }

        ShaderAssetDescriptor shader_asset_descriptor{};

        const auto& stages = asset.at("stages").as_object();

        if (stages.contains("vertex")) {
          ShaderStageDescriptor shader_stage_descriptor{};

          const auto& vertex = stages.at("vertex").as_object();

          if (vertex.contains("spirv")) {
            shader_stage_descriptor.spirvPath = std::string(vertex.at("spirv").as_string());
          }

          if (vertex.contains("meta")) {
            shader_stage_descriptor.metaPath = std::string(vertex.at("meta").as_string());
          }

          shader_asset_descriptor.stages.emplace(
              ShaderStage::Vertex, std::move(shader_stage_descriptor));
        }

        if (stages.contains("fragment")) {
          ShaderStageDescriptor shader_stage_descriptor{};

          const auto& fragment = stages.at("fragment").as_object();

          if (fragment.contains("spirv")) {
            shader_stage_descriptor.spirvPath = std::string(fragment.at("spirv").as_string());
          }

          if (fragment.contains("meta")) {
            shader_stage_descriptor.metaPath = std::string(fragment.at("meta").as_string());
          }

          shader_asset_descriptor.stages.emplace(
              ShaderStage::Fragment, std::move(shader_stage_descriptor));
        }

        iterator->second.data = std::move(shader_asset_descriptor);
      }
    }
  }

  co_return Error::OK;
}

auto AssetManager::getAsset(AssetId asset_id) const
    -> std::expected<const AssetDescriptor*, std::error_code> {

  if (!assets_.contains(asset_id)) {
    return std::unexpected(Error::NotFoundError);
  }
  return &assets_.at(asset_id);
}

}  // namespace gravity