#include "rendering_device.hpp"

#include "source/common/logging/logger.hpp"
#include "source/common/scheduler/scheduler.hpp"
#include "source/platform/window/glfw_window_context.hpp" 
#include "source/rendering/device/vulkan/vulkan_rendering_device.hpp"


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

  VulkanRenderingDevice vulkan_context{ window_context, scheduler.makeStrands<VulkanRenderingDevice>() };
  auto future =
      co_spawn(scheduler.mainExecutor(), vulkan_context.initialize(), boost::asio::use_future);
  future.wait();
  if (auto err = future.get(); err) {
    LOG_ERROR("Failed to initialize vulkan context");
    return err.value();
  }

  VulkanRenderingBackend vulkan_backend{ vulkan_context,
                                         scheduler.makeStrands<VulkanRenderingBackend>() };
  RenderingDevice device{ vulkan_backend, scheduler.makeStrands<RenderingDevice>() };

  future = co_spawn(scheduler.mainExecutor(), device.initialize(), boost::asio::use_future);

  future.wait();
  if (auto err = future.get(); err) {
    LOG_ERROR("Failed to initialize rendering device: {}", err.value());
  }

  LOG_INFO("end");
}