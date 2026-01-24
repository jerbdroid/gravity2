#pragma once

#include "window_context.hpp"

#include "source/platform/glfw_context.hpp"

namespace gravity {
class GlfwWindowContext : public WindowContext {

 public:
  auto initialize() -> std::error_code override;

  [[nodiscard]] auto getResolution() const -> Resolution override;
  [[nodiscard]] auto windowIsOpen() const -> bool override;
  void pollEvents() const override;

  auto getRenderingSurface(RenderingApi rendering_api, void* instance, void* surface)
      -> std::error_code override;

  auto getRenderingExtensionRequirements(RenderingApi rendering_api)
      -> std::unordered_map<std::string, bool> override;

  auto getGlfwWindow() -> GLFWwindow* { return window_; }

 protected:
  friend class IoGlfwContext;
  // entt::dispatcher event_dispatcher_;

 private:
  GLFWwindow* window_{nullptr};
  Resolution resolution_;
};

}  // namespace gravity