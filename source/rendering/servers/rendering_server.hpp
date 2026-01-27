#pragma once

#include "source/common/logging/logger.hpp"
#include "source/common/scheduler/scheduler.hpp"
#include "source/rendering/device/rendering_device.hpp"
#include "source/rendering/resource/resource_manager.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

namespace gravity {

using boost::asio::awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::steady_timer;
using boost::asio::use_awaitable;
using namespace std::chrono_literals;

inline auto task1() -> awaitable<void> {
  auto ex = co_await boost::asio::this_coro::executor;
  steady_timer timer(ex);

  for (int i = 0; i < 3; ++i) {
    std::cout << "task1 tick\n";
    timer.expires_after(1s);
    co_await timer.async_wait(use_awaitable);
  }
}

class RenderingServer {
 public:
  enum class StrandLanes : size_t { Main, _Count };
  using StrandGroup = StrandGroup<RenderingServer>;

  RenderingServer(Scheduler& scheduler, RenderingDevice& device)
      : device_{ device },
        strands_{ scheduler.makeStrands<RenderingServer>() },
        resources_{ scheduler.makeStrands<ResourceManager>() } {}

  auto draw() {
    co_spawn(strands_.getStrand(StrandLanes::Main), task1(), detached);
    co_spawn(strands_.getStrand(StrandLanes::Main), oof(), detached);
    co_spawn(
        strands_.getStrand(StrandLanes::Main), loadShaderModule("shaders/simple_shader.vert.spv"),
        detached);
    co_spawn(strands_.getExecutor(), loadShaderModule("shaders/simple_shader.vert.spv"), detached);
    co_spawn(strands_.getExecutor(), loadShaderModule("shaders/simple_shader.vert.spv"), detached);
    co_spawn(strands_.getExecutor(), loadShaderModule("shaders/simple_shader.vert.spv"), detached);
    co_spawn(strands_.getExecutor(), loadShaderModule("shaders/simple_shader.vert.spv"), detached);
  }

  ~RenderingServer() = default;

  auto oof() -> awaitable<void> {
    BufferDescription description{ .size_ = 100,
                                   .usage_ = BufferUsage::TransferSource,
                                   .visibility_ = Visibility::Device };
    auto handle{ co_await device_.createBuffer(description) };
    co_await device_.destroyBuffer(handle.value());
  }

  auto loadShaderModule(std::string path) -> boost::asio::awaitable<void> {

    ShaderKey key{ .path = path, .stage = ShaderStage::Vertex };

    auto vertHandle = co_await resources_.acquireShader(key);
    // auto  fragHandle = co_await resources_.loadShader(path);
    if (!vertHandle) {
      LOG_ERROR("failed to load shader resource");
      co_return;
    }

    const auto& vert = resources_.getShader(vertHandle.value());
    // const auto& frag = resources_.getShader(fragHandle);

    ShaderDescription description{
      .stage_ = ShaderStage::Vertex,
      .spirv_ = vert.spirv_,
      .entry_point_ = "main",
    };

    co_await device_.createShader(std::move(description));

    co_await resources_.releaseShader(vertHandle.value());
    // resources_.releaseShader(fragHandle);
  }

 private:
  RenderingDevice& device_;

  StrandGroup strands_;
  ResourceManager resources_;
};

}  // namespace gravity