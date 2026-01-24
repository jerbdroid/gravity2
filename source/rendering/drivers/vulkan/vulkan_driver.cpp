#include "vulkan_driver.hpp"

#include <utility>

namespace gravity {

VulkanRenderingDriver::VulkanRenderingDriver(VulkanContext& context, StrandGroup strands)
    : context_{ context }, strands_{ std::move(strands) } {}

auto VulkanRenderingDriver::initialize() -> boost::asio::awaitable<std::error_code> {
  co_return co_await co_spawn(
      strands_.getStrand(StrandLanes::Main), context_.initialize(), boost::asio::use_awaitable);
}

auto VulkanRenderingDriver::prepareBuffers() -> boost::asio::awaitable<std::error_code> {
  co_return co_await co_spawn(
      strands_.getStrand(StrandLanes::Main), context_.prepareBuffers(), boost::asio::use_awaitable);
}

auto VulkanRenderingDriver::swapBuffers() -> boost::asio::awaitable<std::error_code> {
  co_return co_await co_spawn(
      strands_.getStrand(StrandLanes::Main), context_.swapBuffers(), boost::asio::use_awaitable);
}

}  // namespace gravity