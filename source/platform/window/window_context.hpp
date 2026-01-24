#pragma once

#include "source/rendering/common/rendering_api.hpp"

#include <system_error>
#include <unordered_map>

namespace gravity {

struct Resolution {
  uint32_t width_;
  uint32_t height_;
};

class WindowContext {

 public:
  virtual ~WindowContext() = default;

  virtual auto initialize() -> std::error_code = 0;

  [[nodiscard]] virtual auto getResolution() const -> Resolution = 0;
  [[nodiscard]] virtual auto windowIsOpen() const -> bool = 0;
//   [[nodiscard]] virtual auto windowClose() const -> std::error_code = 0;
  virtual void pollEvents() const = 0;

  virtual auto getRenderingSurface(RenderingApi rendering_api, void* instance, void* surface)
      -> std::error_code = 0;

  virtual auto getRenderingExtensionRequirements(RenderingApi rendering_api)
      -> std::unordered_map<std::string, bool> = 0;
};

}  // namespace gravity