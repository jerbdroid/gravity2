#include "rendering_server.hpp"

#include "source/common/logging/logger.hpp"
#include "source/common/scheduler/scheduler.hpp"
#include "source/platform/window/glfw_window_context.hpp"
#include "source/rendering/contexts/vulkan/vulkan_context.hpp"
#include "source/rendering/device/rendering_device.hpp"
#include "source/rendering/drivers/vulkan/vulkan_driver.hpp"

using namespace gravity;
using boost::asio::steady_timer;
using boost::asio::use_awaitable;

using namespace std::chrono_literals;

auto main() -> int {

  if (auto err = setupAsyncLogger(); err) {
    return err.value();
  }

  Scheduler scheduler{};

  GlfwWindowContext window_context{};

  if (auto err = window_context.initialize(); err) {
    LOG_ERROR("Failed to initialize window context");
    return err.value();
  }

  VulkanContext vulkan_context{ window_context };
  VulkanRenderingDriver vulkan_driver{ vulkan_context,
                                       scheduler.makeStrands<VulkanRenderingDriver>() };
  auto future =
      co_spawn(scheduler.mainExecutor(), vulkan_driver.initialize(), boost::asio::use_future);
  future.wait();
  if (auto err = future.get(); err) {
    LOG_ERROR("Failed to initialize rendering device: {}", err.value());
    return err.value();
  }
  RenderingDevice rendering_device{ vulkan_driver, scheduler.makeStrands<RenderingDevice>() };
  RenderingServer rendering_server{ rendering_device, scheduler.makeStrands<RenderingServer>() };

  rendering_server.draw();

  future =
      co_spawn(scheduler.mainExecutor(), rendering_device.initialize(), boost::asio::use_future);

  future.wait();
  if (auto err = future.get(); err) {
    LOG_ERROR("Failed to initialize rendering device: {}", err.value());
  }

  LOG_INFO("end");
}