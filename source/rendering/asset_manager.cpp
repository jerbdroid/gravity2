#include "asset_manager.hpp"
#include <expected>

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

static constexpr const char* TypeParameter{ "type" };
static constexpr const char* NameParameter{ "name" };
static constexpr const char* IdParameter{ "id" };
static constexpr const char* StagesParameter{ "stages" };
static constexpr const char* SpirvParameter{ "spirv" };
static constexpr const char* MetaParameter{ "meta" };
static constexpr const char* ShaderParameter{ "shader" };
static constexpr const char* TexturesParameter{ "textures" };
static constexpr const char* ParametersParameter{ "parameters" };
static constexpr const char* AssetParameter{ "asset" };
static constexpr const char* SamplerParameter{ "sampler" };
static constexpr const char* ImageParameter{ "image" };
static constexpr const char* ColourSpaceParameter{ "colour_space" };
static constexpr const char* MipmapsParameter{ "mipmaps" };

static constexpr const char* SourceParameter{ "source" };
static constexpr const char* SubmeshesParameter{ "submeshes" };
static constexpr const char* FirstIndexParameter{ "first_index" };
static constexpr const char* IndexCountParameter{ "index_count" };
static constexpr const char* MaterialParameter{ "material" };

static constexpr std::array<RequiredParameters, 1> ShaderRequiredParameters{
  { { .name_ = StagesParameter, .expected_type_ = ExpectedTypes::List } }
};

static constexpr std::array<RequiredParameters, 3> ShaderStageRequiredParameters{
  { { .name_ = SpirvParameter, .expected_type_ = ExpectedTypes::String },
    { .name_ = MetaParameter, .expected_type_ = ExpectedTypes::String },
    { .name_ = TypeParameter, .expected_type_ = ExpectedTypes::String } }
};

static constexpr std::array<RequiredParameters, 2> AssetRequiredParameters{
  { { .name_ = IdParameter, .expected_type_ = ExpectedTypes::Integer },
    { .name_ = TypeParameter, .expected_type_ = ExpectedTypes::String } }
};

static constexpr std::array<RequiredParameters, 2> MaterialRequiredParameters{
  { { .name_ = TexturesParameter, .expected_type_ = ExpectedTypes::List },
    { .name_ = ParametersParameter, .expected_type_ = ExpectedTypes::List } }
};

static constexpr std::array<RequiredParameters, 3> MaterialTextureRequiredParameters{
  { { .name_ = NameParameter, .expected_type_ = ExpectedTypes::String },
    { .name_ = AssetParameter, .expected_type_ = ExpectedTypes::Integer },
    { .name_ = SamplerParameter, .expected_type_ = ExpectedTypes::String } }
};

static constexpr std::array<RequiredParameters, 3> TextureRequiredParameters{
  { { .name_ = ImageParameter, .expected_type_ = ExpectedTypes::String },
    { .name_ = ColourSpaceParameter, .expected_type_ = ExpectedTypes::String },
    { .name_ = MipmapsParameter, .expected_type_ = ExpectedTypes::Boolean } }
};

static constexpr std::array<RequiredParameters, 2> MeshRequiredParameters{
  { { .name_ = SourceParameter, .expected_type_ = ExpectedTypes::String },
    { .name_ = SubmeshesParameter, .expected_type_ = ExpectedTypes::List } }
};

static constexpr std::array<RequiredParameters, 4> SubmeshRequiredParameters{
  { { .name_ = NameParameter, .expected_type_ = ExpectedTypes::String },
    { .name_ = FirstIndexParameter, .expected_type_ = ExpectedTypes::Integer },
    { .name_ = IndexCountParameter, .expected_type_ = ExpectedTypes::Integer },
    { .name_ = MaterialParameter, .expected_type_ = ExpectedTypes::Integer } }
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

      auto asset_id{ asset.at(IdParameter).as_int64() };

      auto asset_type{ assetTypeFromString(asset.at(TypeParameter).as_string()) };
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

      switch (iterator->second.type) {
        case AssetType::Shader: {
          auto shader_descriptor_expect{ parseShaderDescriptor(asset) };
          if (!shader_descriptor_expect) {
            LOG_ERROR("corrupt asset database");
            co_return shader_descriptor_expect.error();
          }
          iterator->second.data_ = std::move(shader_descriptor_expect.value());

          break;
        }
        case AssetType::Texture: {
          auto texture_descriptor_expect{ parseTextureDescriptor(asset) };
          if (!texture_descriptor_expect) {
            LOG_ERROR("corrupt asset database");
            co_return texture_descriptor_expect.error();
          }
          iterator->second.data_ = std::move(texture_descriptor_expect.value());

          break;
        }
        case AssetType::Mesh: {
          auto mesh_descriptor_expect{ parseMeshDescriptor(asset) };
          if (!mesh_descriptor_expect) {
            LOG_ERROR("corrupt asset database");
            co_return mesh_descriptor_expect.error();
          }
          iterator->second.data_ = std::move(mesh_descriptor_expect.value());

          break;
        }
        case AssetType::Material: {
          auto material_descriptor_expect{ parseMaterialDescriptor(asset) };
          if (!material_descriptor_expect) {
            LOG_ERROR("corrupt asset database");
            co_return material_descriptor_expect.error();
          }
          iterator->second.data_ = std::move(material_descriptor_expect.value());

          break;
        }
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
      case ExpectedTypes::Boolean:
        if (!object.at(parameter.name_).is_bool()) {
          return Error::SchemaError;
        }
        break;
    }
  }
  return Error::OK;
}

auto AssetManager::parseShaderDescriptor(const boost::json::object& asset)
    -> std::expected<ShaderDescriptor, std::error_code> {
  if (auto error{ validateRequiredParameters(asset, ShaderRequiredParameters) };
      error != Error::OK) {
    return std::unexpected(error);
  }
  const auto& shader_stages{ asset.at(StagesParameter).as_array() };

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

    auto stage{ assetShaderStageFromString(shader_stage.at(TypeParameter).as_string()) };
    if (!stage) {
      return std::unexpected(stage.error());
    }
    shader_descriptor.stages_.emplace(
        stage.value(), ShaderStageDescriptor{
                           .spirv_path_ = std::string(shader_stage.at(SpirvParameter).as_string()),
                           .meta_path_ = std::string(shader_stage.at(MetaParameter).as_string()) });
  }
  return shader_descriptor;
}

auto AssetManager::parseMaterialDescriptor(const boost::json::object& asset)
    -> std::expected<MaterialDescriptor, std::error_code> {
  if (auto error{ validateRequiredParameters(asset, MaterialRequiredParameters) };
      error != Error::OK) {
    return std::unexpected(error);
  }

  const auto& textures{ asset.at(TexturesParameter).as_array() };

  MaterialDescriptor material_descriptor{};

  for (const auto& texture_object : textures) {
    if (!texture_object.is_object()) {
      return std::unexpected(Error::SchemaError);
    }
    const auto& texture = texture_object.as_object();

    if (auto error{ validateRequiredParameters(texture, MaterialTextureRequiredParameters) };
        error != Error::OK) {
      return std::unexpected(error);
    }

    auto& material_texture_descriptor{ material_descriptor.textures_.emplace_back() };
    material_texture_descriptor.name_ = texture.at(NameParameter).as_string();
    material_texture_descriptor.texture_asset_ = texture.at(AssetParameter).as_int64();

    auto sampler_expect{ samplerTypeFromString(texture.at(SamplerParameter).as_string()) };
    if (!sampler_expect) {
      return std::unexpected(sampler_expect.error());
    }

    material_texture_descriptor.sampler_ = sampler_expect.value();
  }

  return material_descriptor;
}

auto AssetManager::parseTextureDescriptor(const boost::json::object& asset)
    -> std::expected<TextureDescriptor, std::error_code> {
  if (auto error{ validateRequiredParameters(asset, TextureRequiredParameters) };
      error != Error::OK) {
    return std::unexpected(error);
  }

  return TextureDescriptor{
    .image_path_ = std::string(asset.at(ImageParameter).as_string()),
    .color_space_ = std::string(asset.at(ColourSpaceParameter).as_string()),
    .mipmaps_ = asset.at(MipmapsParameter).as_bool(),
  };
}

auto AssetManager::parseMeshDescriptor(const boost::json::object& asset)
    -> std::expected<MeshDescriptor, std::error_code> {
  if (auto error{ validateRequiredParameters(asset, MeshRequiredParameters) }; error != Error::OK) {
    return std::unexpected(error);
  }

  const auto& submeshes{ asset.at(SubmeshesParameter).as_array() };

  MeshDescriptor mesh_descriptor{ .source_ = std::string(asset.at(SourceParameter).as_string()) };
  for (const auto& submesh_object : submeshes) {
    if (!submesh_object.is_object()) {
      return std::unexpected(Error::SchemaError);
    }
    const auto& submesh = submesh_object.as_object();

    if (auto error{ validateRequiredParameters(submesh, SubmeshRequiredParameters) };
        error != Error::OK) {
      return std::unexpected(error);
    }

    ;
    mesh_descriptor.submeshes_.emplace_back(
        SubmeshDescriptor{
            .name_ = std::string(submesh.at(NameParameter).as_string()),
            .first_index_ = submesh.at(FirstIndexParameter).as_int64(),
            .index_count_ = submesh.at(IndexCountParameter).as_int64(),
            .material_asset_ = submesh.at(MaterialParameter).as_int64(),

        });
  }

  return mesh_descriptor;
}

}  // namespace gravity