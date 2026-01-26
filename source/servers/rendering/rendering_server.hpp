#pragma once

#include "source/common/logging/logger.hpp"
#include "source/common/scheduler/scheduler.hpp"
#include "source/rendering/device/rendering_device.hpp"

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

  RenderingServer(RenderingDevice& device, StrandGroup strands)
      : device_{ device }, strands_{ std::move(strands) } {}

  auto draw() {
    co_spawn(strands_.getStrand(StrandLanes::Main), task1(), detached);
    co_spawn(strands_.getStrand(StrandLanes::Main), oof(), detached);
    loadShaderModule("shaders/simple_shader.vert.spv");
  }

  ~RenderingServer() = default;

  auto oof() -> awaitable<void> {
    BufferDescription description{ .size_ = 100,
                                   .usage_ = BufferUsage::TransferSource,
                                   .visibility_ = Visibility::Device };
    auto handle{ co_await device_.createBuffer(description) };
    co_await device_.destroyBuffer(handle.value());
  }

  void loadShaderModule(const std::string& path) {
    auto file = std::make_shared<boost::asio::stream_file>(
        strands_.getExecutor(), path, boost::asio::stream_file::read_only);

    auto dynamic_buf = std::make_shared<boost::asio::streambuf>();

    boost::asio::async_read(
        *file, *dynamic_buf, boost::asio::transfer_all(),
        boost::asio::bind_executor(
            strands_.getStrand(StrandLanes::Main),
            [file, dynamic_buf, this](const boost::system::error_code& ec, std::size_t bytes) {
              if (ec == boost::asio::error::eof || !ec) {
                std::istream is(dynamic_buf.get());
                std::vector<char> buffer(
                    (std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
                if (buffer.size() % 4 != 0) {
                  LOG_ERROR("Shader file size is not multiple of 4 bytes");
                  return;
                }
                std::vector<uint32_t> spirv(buffer.size() / 4);
                std::memcpy(spirv.data(), buffer.data(), buffer.size());

                ShaderDescription description{
                  .stage_ = ShaderStage::Vertex,
                  .spirv_ = std::move(spirv),
                  .entry_point_ = "main",
                };

                boost::asio::co_spawn(
                    strands_.getStrand(StrandLanes::Main),
                    device_.createShader(std::move(description)), boost::asio::detached);
              } else {
                LOG_ERROR("shader module read error: {}", ec.message());
              }
            }));
  }

 private:
  RenderingDevice& device_;
  StrandGroup strands_;
};

}  // namespace gravity