#pragma once

#include "source/common/scheduler/scheduler.hpp"
#include "source/rendering/drivers/rendering_driver.hpp"

using boost::asio::awaitable;

namespace gravity {

class RenderingDevice {
 public:
  using StrandGroup = StrandGroup<RenderingDevice>;
  enum class StrandLanes : size_t { Main, _Count };

  ~RenderingDevice() = default;

  RenderingDevice(RenderingDriver& driver, StrandGroup strands);

  auto initialize() -> awaitable<std::error_code>;

 private:
  auto do_initialize() -> awaitable<std::error_code>;

  RenderingDriver& driver_;
  StrandGroup strands_;
};

}  // namespace gravity