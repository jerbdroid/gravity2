#pragma once

#include "boost/asio.hpp"

namespace gravity {

class RenderingDriver {
 public:
  virtual ~RenderingDriver() = default;

  virtual auto initialize() -> boost::asio::awaitable<std::error_code> = 0;
  virtual auto prepareBuffers() -> boost::asio::awaitable<std::error_code> = 0;
  virtual auto swapBuffers() -> boost::asio::awaitable<std::error_code> = 0;
};

}  // namespace gravity