#pragma once

#include "source/common/scheduler/scheduler.hpp"
#include "source/rendering/device/rendering_device.hpp"

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

  RenderingServer(RenderingDevice& device, StrandGroup strands)
      : device_{ device }, strands_{ std::move(strands) } {}

  auto draw() {
    co_spawn(strands_.getStrand(StrandLanes::Main), task1(), detached);
    co_spawn(strands_.getStrand(StrandLanes::Main), oof(), detached);
  }

  ~RenderingServer() = default;

  auto oof() -> awaitable<void> {
    BufferDescription description{ .size_ = 100,
                                   .usage_ = BufferUsage::TransferSource,
                                   .visibility_ = BufferVisibility::Device };
    auto handle{ co_await device_.createBuffer(description) };
    co_await device_.destroyBuffer(handle.value());
  }

 private:
  RenderingDevice& device_;
  StrandGroup strands_;
};

}  // namespace gravity