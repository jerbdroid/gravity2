#pragma once

#include "source/rendering/common/rendering_api.hpp"

#include "boost/asio.hpp"

#include <cstddef>
#include <expected>

namespace gravity {

struct BufferDescription {
  size_t size_;
  BufferUsage usage_;
  BufferVisibility visibility_;
};

struct BufferHandle {
  size_t index_;
  size_t generation_;
};

class RenderingDevice {
 public:
  virtual ~RenderingDevice() = default;

  virtual auto createBuffer(BufferDescription description)
      -> boost::asio::awaitable<std::expected<BufferHandle, std::error_code>> = 0;
  virtual auto destroyBuffer(BufferHandle buffer_handle) -> boost::asio::awaitable<std::error_code> = 0;
  virtual auto initialize() -> boost::asio::awaitable<std::error_code> = 0;
};

}  // namespace gravity