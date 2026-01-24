#pragma once

#include "boost/asio.hpp"

namespace gravity {

class RenderingDevice {
 public:
  virtual ~RenderingDevice() = default;

  virtual auto initialize() -> boost::asio::awaitable<std::error_code> = 0;
};

}  // namespace gravity