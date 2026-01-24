#pragma once

#include <cstdint>
#include "source/common/scheduler/scheduler.hpp"
#include "source/rendering/contexts/vulkan/vulkan_context.hpp"
#include "source/rendering/drivers/rendering_driver.hpp"

namespace gravity {

class VulkanRenderingDriver : public RenderingDriver {
 public:
  enum class StrandLanes : uint8_t { Main, _Count };
  using StrandGroup = StrandGroup<VulkanRenderingDriver>;

  ~VulkanRenderingDriver() = default;

  VulkanRenderingDriver(VulkanContext& context, StrandGroup strands);

  auto initialize() -> boost::asio::awaitable<std::error_code> override;
  auto prepareBuffers() -> boost::asio::awaitable<std::error_code> override;
  auto swapBuffers() -> boost::asio::awaitable<std::error_code> override;

 private:
  VulkanContext& context_;
  StrandGroup strands_;
};

}  // namespace gravity