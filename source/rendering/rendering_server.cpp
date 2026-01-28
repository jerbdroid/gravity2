#include "rendering_server.hpp"

#include "source/common/logging/logger.hpp"

namespace gravity {

auto RenderingServer::draw() -> boost::asio::awaitable<void> {
  co_spawn(
      strands_.getStrand(StrandLanes::Main), loadShaderModule("shaders/simple_shader.vert.spv"),
      boost::asio::detached);
  co_spawn(
      strands_.getExecutor(), loadShaderModule("shaders/simple_shader.vert.spv"),
      boost::asio::detached);
  co_spawn(
      strands_.getExecutor(), loadShaderModule("shaders/simple_shader.vert.spv"),
      boost::asio::detached);
  co_spawn(
      strands_.getExecutor(), loadShaderModule("shaders/simple_shader.vert.spv"),
      boost::asio::detached);
  co_spawn(
      strands_.getExecutor(), loadShaderModule("shaders/simple_shader.vert.spv"),
      boost::asio::detached);

  co_return;
}

auto RenderingServer::loadShaderModule(std::string path) -> boost::asio::awaitable<void> {

  ShaderResourceDescription shader_resource_description{ .path = path };

  auto vertHandle = co_await resources_.acquireShaderResource(shader_resource_description);
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
    .hash_ = vert.hash_,
  };

  co_await device_.createShader(description);

  co_await resources_.releaseShaderResource(vertHandle.value());
  // resources_.releaseShaderResource(fragHandle);
}

}  // namespace gravity