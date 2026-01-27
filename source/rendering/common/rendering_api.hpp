#pragma once

#include "source/common/templates/bitmask.hpp"

#include <cstdint>

namespace gravity {

enum class RenderingApi : std::uint8_t { Vulkan };

enum class Format : uint8_t {
  Undefined = 0,
  ColorRgba8UnsignedNormalized = 1,
  VectorXyzw8UnsignedNormalized = 1,
  ColorRgba8SignedNormalized = 2,
  ColorRgba8sRgb = 3,
  ColorRg32SignedFloat = 4,
  ColorRgb32SignedFloat = 5,
  ColorRgba32UnsignedInt = 6,
  Depth32SignedFloat = 8,
  Depth24UnsignedNormalizedStencil8UnsignedInteger = 9,
  Depth32SignedFloatStencil8UnsignedInt = 10
};

enum class BufferUsage : uint16_t {
  TransferSource = (1U << 0U),
  TransferDestination = (1U << 1U),
  ReadOnlyTexel = (1U << 2U),
  ReadWriteTexel = (1U << 3U),
  ReadOnly = (1U << 4U),
  ReadWrite = (1U << 5U),
  Index = (1U << 6U),
  Vertex = (1U << 7U),
  Indirect = (1U << 8U),
};

constexpr auto enable_bitmask_operators(BufferUsage) -> bool;

enum class Visibility : uint8_t { Host, Device };

enum class ImageType : uint8_t { Linear, Plane, Cube };
enum class ImageSamples : uint8_t { S1, S2, S4, S8, S16, S32, S64 };

enum class ImageUsage : uint8_t {
  TransferSource = (1U << 0U),
  TransferDestination = (1U << 1U),
  Sampled = (1U << 2U),
  ColorAttachment = (1U << 4U),
  DepthStencilAttachment = (1U << 5U),
};

constexpr auto enable_bitmask_operators(ImageUsage) -> bool;

enum class SamplerFilter : uint8_t {
  Nearest,
  Linear,
  Cubic,
};

enum class SamplerMipMapMode : uint8_t {
  Nearest,
  Linear,
};

enum class SamplerAddressMode : uint8_t {
  Repeat,
  MirroredRepeat,
  ClampToEdge,
  ClampToBorder,
  MirrorClampToEdge,
};

enum class CompareOperation : uint8_t {
  Never,
  Less,
  Equal,
  LessOrEqual,
  Greater,
  NotEqual,
  GreaterOrEqual,
  Always
};

enum class BorderColor : uint8_t { FloatOpaqueBlack };

enum class ShaderStage : uint8_t {
  Unknown = 0,
  Vertex = (1U << 0U),
  Fragment = (1U << 1U),
  Compute = (1U << 2U),
  Geometry = (1U << 3U),
  TesselationControl = (1U << 4U),
  TesselationEvaluation = (1U << 5U),
};

constexpr auto enable_bitmask_operators(ShaderStage) -> bool;

enum class VertexFormat : uint8_t {
  Float1,
  Float2,
  Float3,
  Float4,
  Uint32,
};

enum class VertexInputRate : uint8_t {
  Vertex,
  Instance,
};

}  // namespace gravity