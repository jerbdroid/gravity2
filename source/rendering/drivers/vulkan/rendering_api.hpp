#include "source/rendering/common/rendering_api.hpp"

#include "vulkan/vulkan_raii.hpp"

namespace gravity {

static constexpr std::array<VkFormat, 11> FormatTable = {
  VK_FORMAT_UNDEFINED,
  VK_FORMAT_R8G8B8A8_UNORM,
  VK_FORMAT_R8G8B8A8_SNORM,
  VK_FORMAT_R8G8B8A8_SRGB,
  VK_FORMAT_R32G32_SFLOAT,
  VK_FORMAT_R32G32B32_SFLOAT,
  VK_FORMAT_R32G32B32A32_UINT,
  VK_FORMAT_R32G32B32A32_SFLOAT,
  VK_FORMAT_D32_SFLOAT,
  VK_FORMAT_D24_UNORM_S8_UINT,
  VK_FORMAT_D32_SFLOAT_S8_UINT,
};

constexpr auto convert(Format format) -> VkFormat {
  return FormatTable[static_cast<size_t>(format)];
}

}  // namespace gravity