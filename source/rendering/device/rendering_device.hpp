#pragma once

#include "source/common/utilities.hpp"
#include "source/rendering/common/rendering_type.hpp"

#include "boost/asio/awaitable.hpp"

#include <cstddef>
#include <expected>
#include <span>
#include <system_error>

namespace gravity {

struct Extent {
  uint32_t width_;
  uint32_t height_;
  uint32_t depth_;
};

struct BufferDescriptor {
  size_t size_;
  BufferUsage usage_;
  Visibility visibility_;
};

struct ImageDescriptor {
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

struct SamplerDescriptor {
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

struct ShaderModuleDescriptor {
  ShaderStage stage_ = ShaderStage::Vertex;
  std::span<const uint32_t> spirv_;
  HashType hash_;
};

struct ShaderModuleHandle {
  size_t index_ = 0;
  size_t generation_ = 0;
};

class RenderingDevice {
 public:
  virtual ~RenderingDevice() = default;

  virtual auto initialize() -> boost::asio::awaitable<std::error_code> = 0;

  virtual auto createBuffer(const BufferDescriptor& descriptor)
      -> boost::asio::awaitable<std::expected<BufferHandle, std::error_code>> = 0;
  virtual auto destroyBuffer(BufferHandle buffer_handle)
      -> boost::asio::awaitable<std::error_code> = 0;

  virtual auto createImage(const ImageDescriptor& descriptor)
      -> boost::asio::awaitable<std::expected<ImageHandle, std::error_code>> = 0;
  virtual auto destroyImage(ImageHandle image_handle)
      -> boost::asio::awaitable<std::error_code> = 0;

  virtual auto createSampler(const SamplerDescriptor& descriptor)
      -> boost::asio::awaitable<std::expected<SamplerHandle, std::error_code>> = 0;
  virtual auto destroySampler(SamplerHandle sampler_handler)
      -> boost::asio::awaitable<std::error_code> = 0;

  virtual auto createShaderModule(ShaderModuleDescriptor descriptor)
      -> boost::asio::awaitable<std::expected<ShaderModuleHandle, std::error_code>> = 0;
  virtual auto destroyShaderModule(ShaderModuleHandle shader_handle)
      -> boost::asio::awaitable<std::error_code> = 0;
};

}  // namespace gravity