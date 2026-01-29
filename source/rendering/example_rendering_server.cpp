#include "rendering_server.hpp"

#include "source/common/logging/logger.hpp"
#include "source/common/scheduler/scheduler.hpp"
#include "source/platform/window/glfw_window_context.hpp"
#include "source/rendering/device/vulkan/vulkan_rendering_device.hpp"

#include "boost/asio/detached.hpp"

#include <iostream>
#include <thread>

using namespace gravity;
using boost::asio::steady_timer;
using boost::asio::use_awaitable;

using namespace std::chrono_literals;

namespace boost {

void throw_exception(const std::exception& e, const boost::source_location&) {
  std::cerr << "Boost exception: " << e.what() << "\n";
  std::abort();
}

void throw_exception(stdext::exception const& e) {
  std::cerr << "Boost exception: " << e.what() << "\n";
  std::abort();
}

}  // namespace boost

auto main() -> int {

  if (auto err = setupAsyncLogger(); err) {
    return err.value();
  }

  gravity::getOrCreateLogger("vulkan")->set_level(spdlog::level::trace);
  gravity::getOrCreateLogger("scheduler")->set_level(spdlog::level::err);

  Scheduler scheduler{};

  GlfwWindowContext window_context{};

  if (auto err = window_context.initialize(); err) {
    LOG_ERROR("Failed to initialize window context");
    return err.value();
  }

  VulkanRenderingDevice vulkan_rendering_device{ window_context,
                                                 scheduler.makeStrands<VulkanRenderingDevice>() };
  auto future = co_spawn(
      scheduler.getStrand(Scheduler::StrandLanes::Main), vulkan_rendering_device.initialize(),
      boost::asio::use_future);
  future.wait();
  if (auto err = future.get(); err) {
    LOG_ERROR("Failed to initialize rendering device: {}", err.value());
    return err.value();
  }
  RenderingServer rendering_server{
    scheduler,
    vulkan_rendering_device,
  };

  future = co_spawn(
      scheduler.getStrand(Scheduler::StrandLanes::Main), rendering_server.initialize(),
      boost::asio::use_future);
  future.wait();
  if (auto err = future.get(); err) {
    LOG_ERROR("Failed to initialize rendering device: {}", err.value());
    return err.value();
  }

  co_spawn(
      scheduler.getStrand(Scheduler::StrandLanes::Main), rendering_server.draw(),
      boost::asio::detached);

  std::this_thread::sleep_for(5s);

  LOG_INFO("end");
}