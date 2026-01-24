#pragma once

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

}  // namespace gravity