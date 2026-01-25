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
  VectorXy32SignedFloat = 4,
  ColorRg32SignedFloat = 4,
  ColorRgb32SignedFloat = 5,
  VectorXyz32SignedFloat = 5,
  ColorRgba32UnsignedInt = 6,
  VectorXyzw32SignedFloat = 7,
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

enum class BufferVisibility : uint8_t { Host, Device };

}  // namespace gravity