#include "rendering_device.hpp"

#include "source/common/logging/logger.hpp"

namespace gravity {

using boost::asio::co_spawn;
using boost::asio::use_awaitable;
using boost::asio::this_coro::executor;

RenderingDevice::RenderingDevice(RenderingDriver& driver, StrandGroup strands)
    : driver_{ driver }, strands_{ std::move(strands) } {}

auto RenderingDevice::initialize() -> awaitable<std::error_code> {
  co_return co_await co_spawn(
      strands_.getStrand(StrandLanes::Main), do_initialize(), use_awaitable);
}

auto RenderingDevice::do_initialize() -> awaitable<std::error_code> {
  LOG_TRACE("RenderingDevice initialized");
  co_return Error::UnimplementedError;
}

}  // namespace gravity