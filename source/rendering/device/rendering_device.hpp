#pragma once

#include "boost/asio/awaitable.hpp"
#include "source/rendering/common/rendering_api.hpp"

#include "boost/asio.hpp"

#include <cstddef>
#include <expected>
#include <system_error>
#include <span>

namespace gravity {

struct Extent {
  uint32_t width_;
  uint32_t height_;
  uint32_t depth_;
};

struct BufferDescription {
  size_t size_;
  BufferUsage usage_;
  Visibility visibility_;
};

struct ImageDescription {
  Extent extent_ = { .width_ = 0, .height_ = 0, .depth_ = 1 };
  uint32_t layers_ = 1;
  uint32_t mip_level_ = 1;
  Format format_ = Format::ColorRgba8UnsignedNormalized;
  ImageType type_ = ImageType::Plane;
  ImageSamples samples_ = ImageSamples::S1;
  Visibility visibility_ = Visibility::Device;
  ImageUsage usage_ = ImageUsage::Sampled;
};

struct BufferHandle {
  size_t index_;
  size_t generation_;
};

struct ImageHandle {
  size_t index_;
  size_t generation_;
};

struct VertexAttribute {
  uint32_t location;
  VertexFormat format;
  uint32_t offset;
};

struct VertexBinding {
  uint32_t binding;
  uint32_t stride;
  VertexInputRate input_rate;
};

struct VertexLayout {
  std::vector<VertexBinding> bindings;
  std::vector<VertexAttribute> attributes;
};

struct SamplerDescription {
  SamplerFilter magnification_filter_;
  SamplerFilter minification_filter_;
  SamplerMipMapMode mipmap_mode_;

  SamplerAddressMode address_mode_u_;
  SamplerAddressMode address_mode_v_;
  SamplerAddressMode address_mode_w_;

  CompareOperation comparison_operation_ = CompareOperation::Never;
  BorderColor border_color_ = BorderColor::FloatOpaqueBlack;

  float mip_lod_bias_ = 0.0F;
  float min_lod_ = 0.0F;
  float max_lod_ = 0.0F;
  float max_anisotropy_ = 0.0F;

  bool anisotropy_enabled_ = false;
  bool compare_enabled_ = false;
};

struct SamplerHandle {
  size_t index_;
  size_t generation_;
};

struct ShaderDescription {
  ShaderStage stage_ = ShaderStage::Unknown;
  std::span<const uint32_t> spirv_;
  std::string entry_point_ = "main";
};

struct ShaderHandle {
  size_t index_;
  size_t generation_;
};

class RenderingDevice {
 public:
  virtual ~RenderingDevice() = default;

  virtual auto initialize() -> boost::asio::awaitable<std::error_code> = 0;

  virtual auto createBuffer(const BufferDescription& description)
      -> boost::asio::awaitable<std::expected<BufferHandle, std::error_code>> = 0;
  virtual auto destroyBuffer(BufferHandle buffer_handle)
      -> boost::asio::awaitable<std::error_code> = 0;

  virtual auto createImage(const ImageDescription& description)
      -> boost::asio::awaitable<std::expected<ImageHandle, std::error_code>> = 0;
  virtual auto destroyImage(ImageHandle image_handle)
      -> boost::asio::awaitable<std::error_code> = 0;

  virtual auto createSampler(const SamplerDescription& description)
      -> boost::asio::awaitable<std::expected<SamplerHandle, std::error_code>> = 0;
  virtual auto destroySampler(SamplerHandle sampler_handler)
      -> boost::asio::awaitable<std::error_code> = 0;

  virtual auto createShader(ShaderDescription description)
      -> boost::asio::awaitable<std::expected<ShaderHandle, std::error_code>> = 0;
  virtual auto destroyShader(ShaderHandle shader_handle)
      -> boost::asio::awaitable<std::error_code> = 0;
};

}  // namespace gravity