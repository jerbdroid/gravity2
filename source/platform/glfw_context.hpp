#pragma once

#include "source/common/logging/logger.hpp"

#include "vulkan/vulkan_raii.hpp"

#include "GLFW/glfw3.h"

#include <memory>

namespace gravity {

struct GLFWContext {

  static auto createGlfwContext() -> std::shared_ptr<GLFWContext> {
    static auto glfw_context{ std::shared_ptr<GLFWContext>(new GLFWContext()) };
    return glfw_context;
  }

  ~GLFWContext() { glfwTerminate(); }

  GLFWContext(const GLFWContext&) = delete;
  GLFWContext(GLFWContext&&) = delete;
  auto operator=(const GLFWContext&) -> GLFWContext& = delete;
  auto operator=(GLFWContext&&) -> GLFWContext& = delete;

 private:
  GLFWContext() {
    glfwInit();
    glfwSetErrorCallback([](int /*error*/, const char* msg) { LOG_ERROR("glfw: {}", msg); });
  }
};

using GLFWContextSharedPtr = std::shared_ptr<GLFWContext>;

}  // namespace gravity