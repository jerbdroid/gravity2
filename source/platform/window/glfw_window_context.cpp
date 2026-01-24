#include "glfw_window_context.hpp"

#include "vulkan/vulkan_raii.hpp"

#include "GLFW/glfw3.h"

namespace gravity {

auto GlfwWindowContext::initialize() -> std::error_code {
  GLFWContext::createGlfwContext();

  // const auto& config{WindowConfig::getInstance()};

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  // if (config.isResizable()) {
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  // } else {
  //   glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  // }

  glfwWindowHint(GLFW_SAMPLES, 0);

  // RELEASE_ASSERT(config.width() <= std::numeric_limits<int>::max(), "width resolution too
  // large"); RELEASE_ASSERT(config.height() <= std::numeric_limits<int>::max(), "height resolution
  // too large");
  // int width{static_cast<int>(config.width())};
  // int height{static_cast<int>(config.height())};
  int width{1280};
  int height{720};

  window_ = glfwCreateWindow(width, height, /*config.title().c_str()*/ "gravity", nullptr, nullptr);

  if (window_ == nullptr) {
    LOG_ERROR("failed to create glfw window");
    return Error::InternalError;
  }

  int pixel_width_count{0};
  int pixel_height_count{0};
  glfwGetFramebufferSize(window_, &pixel_width_count, &pixel_height_count);
  resolution_ = {.width_ = static_cast<uint32_t>(pixel_width_count),
                 .height_ = static_cast<uint32_t>(pixel_height_count)};

  if (resolution_.width_ <= 0 || resolution_.height_ <= 0) {
    LOG_ERROR("invalid window resolution");
    return Error::InternalError;
  }

  glfwSetWindowUserPointer(window_, this);

  if (glfwRawMouseMotionSupported() != 0) {
    glfwSetInputMode(window_, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
  }

  glfwSetFramebufferSizeCallback(
      // NOLINTNEXTLINE (bugprone-easily-swappable-parameters)
      window_, [](GLFWwindow* window, int width, int height) {
        auto* context{static_cast<GlfwWindowContext*>(glfwGetWindowUserPointer(window))};

        assert(height >= 0);
        assert(width >= 0);

        // TODO(jerbdroid): Trigger event here

        // context->setWindowPixelSize({static_cast<uint32_t>(width),
        // static_cast<uint32_t>(height)});

        /*
        auto &window_handler{
            context->graph_.getSceneWriter().get<component::WindowHandler>(
                context->entity_)};

        window_handler.width_ = width;
        window_handler.height_ = height;

        for (auto &callback :
             window_handler.window_resize_listener_callbacks_) {
          callback();
        }
        */
      });

  return Error::OK;
}

auto GlfwWindowContext::getResolution() const -> Resolution {
  return resolution_;
}

auto GlfwWindowContext::windowIsOpen() const -> bool {
  return glfwWindowShouldClose(window_) == 0;
}

void GlfwWindowContext::pollEvents() const {
  glfwPollEvents();
  // event_dispatcher_.update();
}

auto GlfwWindowContext::getRenderingSurface(RenderingApi rendering_api, void* instance,
                                            void* surface) -> std::error_code {

  switch (rendering_api) {
    case RenderingApi::Vulkan: {
      auto result{glfwCreateWindowSurface(static_cast<VkInstance>(instance), window_, nullptr,
                                          static_cast<VkSurfaceKHR*>(surface))};

      if (result != VK_SUCCESS) {
        LOG_ERROR("unable to create vulkan window surface");
        return Error::InternalError;
      }

      return Error::OK;
    } break;
  }

  LOG_ERROR("surface not found for specified api");
  return Error::NotFoundError;
}

auto GlfwWindowContext::getRenderingExtensionRequirements(RenderingApi rendering_api)
    -> std::unordered_map<std::string, bool> {

  std::unordered_map<std::string, bool> extensions;

  switch (rendering_api) {
    case RenderingApi::Vulkan: {
      uint32_t extension_count{0};

      const auto** glfw_required_extensions{glfwGetRequiredInstanceExtensions(&extension_count)};

      for (const auto* const extension :
           std::span<const char*>(glfw_required_extensions, extension_count)) {
        extensions.emplace(extension, true);
      }

    } break;
  }

  return extensions;
}

}  // namespace gravity