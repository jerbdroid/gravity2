#include "asset_manager.hpp"

#include "boost/json/array.hpp"
#include "common/asset_types.hpp"
#include "source/common/error.hpp"
#include "source/common/logging/logger.hpp"

#include "boost/asio.hpp"
#include "boost/json.hpp"

#undef GRAVITY_MODULE_NAME
#define GRAVITY_MODULE_NAME "resource_manager"

namespace json = boost::json;

namespace gravity {

static constexpr const char* AssetTypeParameter{ "type" };
static constexpr const char* AssetIdParameter{ "id" };

static constexpr const char* ShaderStageParameter{ "stages" };

static constexpr const char* ShaderStageSpirvParameter{ "spirv" };
static constexpr const char* ShaderStageMetaParameter{ "meta" };
static constexpr const char* ShaderStageTypeParameter{ "type" };

static constexpr std::array<RequiredParameters, 1> ShaderRequiredParameters{
  { { .name_ = ShaderStageParameter, .expected_type_ = ExpectedTypes::List } }
};

static constexpr std::array<RequiredParameters, 3> ShaderStageRequiredParameters{
  { { .name_ = ShaderStageSpirvParameter, .expected_type_ = ExpectedTypes::String },
    { .name_ = ShaderStageMetaParameter, .expected_type_ = ExpectedTypes::String },
    { .name_ = ShaderStageTypeParameter, .expected_type_ = ExpectedTypes::String } }
};

static constexpr std::array<RequiredParameters, 2> AssetRequiredParameters{
  { { .name_ = AssetIdParameter, .expected_type_ = ExpectedTypes::Integer },
    { .name_ = AssetTypeParameter, .expected_type_ = ExpectedTypes::String } }
};

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

      if (auto error{ validateRequiredParameters(asset, AssetRequiredParameters) };
          error != Error::OK) {
        co_return error;
      }

      auto asset_id{ asset.at(AssetIdParameter).as_int64() };

      auto asset_type{ assetTypeFromString(asset.at(AssetTypeParameter).as_string()) };
      if (!asset_type) {
        LOG_ERROR("invalid asset type");
        co_return asset_type.error();
      }

      auto [iterator, inserted] =
          assets_.emplace(asset_id, AssetDescriptor{ .type = asset_type.value() });

      if (!inserted) {
        LOG_ERROR("duplicate asset id {}", asset_id);
        co_return Error::SchemaError;
      }

      if (iterator->second.type == AssetType::Shader) {
        if (auto error{ validateRequiredParameters(asset, ShaderRequiredParameters) };
            error != Error::OK) {
          co_return error;
        }
        auto shader_descriptor_expect{ parseShaderDescriptor(
            asset.at(ShaderStageParameter).as_array()) };
        if (!shader_descriptor_expect) {
          LOG_ERROR("asset database ");
          co_return shader_descriptor_expect.error();
        }

        iterator->second.data = std::move(shader_descriptor_expect.value());
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

auto AssetManager::validateRequiredParameters(
    const boost::json::object& object, std::span<const RequiredParameters> parameters)
    -> std::error_code {
  for (const auto& parameter : parameters) {
    if (!object.contains(parameter.name_)) {
      return Error::SchemaError;
    }

    switch (parameter.expected_type_) {
      case ExpectedTypes::String:
        if (!object.at(parameter.name_).is_string()) {
          return Error::SchemaError;
        }
        break;
      case ExpectedTypes::Integer:
        if (!object.at(parameter.name_).is_int64()) {
          return Error::SchemaError;
        }
        break;
      case ExpectedTypes::List:
        if (!object.at(parameter.name_).is_array()) {
          return Error::SchemaError;
        }
        break;
    }
  }
  return Error::OK;
}

auto AssetManager::parseShaderDescriptor(const boost::json::array& shader_stages)
    -> std::expected<ShaderDescriptor, std::error_code> {
  ShaderDescriptor shader_descriptor{};

  for (const auto& shader_stage_object : shader_stages) {
    if (!shader_stage_object.is_object()) {
      return std::unexpected(Error::SchemaError);
    }
    const auto& shader_stage = shader_stage_object.as_object();
    if (auto error{ validateRequiredParameters(shader_stage, ShaderStageRequiredParameters) };
        error != Error::OK) {
      return std::unexpected(error);
    }

    auto stage{ assetShaderStageFromString(shader_stage.at(ShaderStageTypeParameter).as_string()) };
    if (!stage) {
      return std::unexpected(stage.error());
    }
    shader_descriptor.stages.emplace(
        stage.value(),
        ShaderStageDescriptor{
            .spirvPath = std::string(shader_stage.at(ShaderStageSpirvParameter).as_string()),
            .metaPath = std::string(shader_stage.at(ShaderStageMetaParameter).as_string()) });
  }

  return shader_descriptor;
}

}  // namespace gravity