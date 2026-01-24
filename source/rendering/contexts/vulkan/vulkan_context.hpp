#pragma once

#include "descriptor_allocator.hpp"

#include "source/common/scheduler/scheduler.hpp"
#include "source/platform/window/window_context.hpp"

#include "vma/vk_mem_alloc.h"
#include "vulkan/vulkan_raii.hpp"

#include "vulkan/vulkan_handles.hpp"

#include <optional>
#include <unordered_set>
#include <vector>

namespace gravity {

struct ImageData {
  vk::UniqueImageView view_{ nullptr };
  vk::ImageCreateInfo image_info_;
  vk::ImageViewCreateInfo view_info_;
  VmaAllocator allocator_{ nullptr };
  VmaAllocation allocation_{ nullptr };
  VmaAllocationInfo allocation_info_{};

  ImageData() = default;
  ImageData(const ImageData&) = delete;

  ImageData(ImageData&& other) noexcept { *this = std::move(other); };

  auto operator=(const ImageData&) -> ImageData& = delete;

  auto operator=(ImageData&& other) -> ImageData& = default;

  ~ImageData() {
    if (view_info_.image != nullptr && allocator_ != nullptr) {
      vmaDestroyImage(allocator_, view_info_.image, allocation_);
      view_info_.image = nullptr;
    }
  }
};

class VulkanContext {
 public:
  ~VulkanContext() = default;
  VulkanContext(WindowContext& window_context);

  auto initialize() -> boost::asio::awaitable<std::error_code>;
  auto prepareBuffers() -> boost::asio::awaitable<std::error_code>;
  auto swapBuffers() -> boost::asio::awaitable<std::error_code>;

 private:
  struct FrameSync {
    std::optional<vk::raii::Semaphore> image_available_;
    std::optional<vk::raii::Semaphore> render_finished_;
    std::optional<vk::raii::Fence> in_flight_;
    std::optional<vk::raii::CommandPool> command_pool_;
    std::vector<vk::raii::CommandBuffer> command_buffers_;
  };

  struct SwapchainResources {
    std::optional<vk::raii::SwapchainKHR> swapchain_;
    std::vector<vk::raii::Framebuffer> framebuffers_;
    std::vector<vk::raii::ImageView> images_;

    uint32_t current_buffer_;
  };

  WindowContext& window_context_;

  vk::raii::Context vk_context_;

  std::optional<vk::raii::Instance> instance_;

  // device
  std::optional<vk::raii::PhysicalDevice> physical_device_;
  // vk::UniqueDevice device_;
  std::optional<vk::raii::Device> device_;

  // data related to output image and surface
  std::optional<vk::raii::SurfaceKHR> surface_;
  vk::SurfaceFormatKHR surface_format_;

  // swapchain
  SwapchainResources swapchain_resources_;

  // render pass
  std::optional<vk::raii::RenderPass> render_pass_;

  // queues
  bool separate_queues_{ false };
  uint32_t graphics_family_queue_index_{ std::numeric_limits<uint32_t>::max() };
  uint32_t present_family_queue_index_{ std::numeric_limits<uint32_t>::max() };
  std::optional<vk::raii::Queue> graphics_queue_;
  std::optional<vk::raii::Queue> present_queue_;

  // synchronization
  std::array<FrameSync, 2> frames_;
  std::array<vk::raii::Fence*, 2> frames_in_flight_;
  size_t current_frame_{ 0 };

  // cache
  std::optional<vk::raii::PipelineCache> pipeline_cache_;

  // descriptor allocator
  std::unique_ptr<DescriptorAllocatorPool> descriptor_allocator_static_;

  // memory
  VmaAllocator memory_allocator_;

  // dynamic loader for EXT
  vk::detail::DispatchLoaderDynamic dynamic_dispatcher_;

  std::unordered_set<std::string> enabled_instance_extension_names_;
  std::unordered_set<std::string> enabled_instance_layer_names_;
  std::unordered_set<std::string> enabled_device_extension_names_;

  auto initializeVulkanInstance() -> boost::asio::awaitable<std::error_code>;
  auto initializeSurface() -> boost::asio::awaitable<std::error_code>;
  auto initializePhysicalDevice() -> boost::asio::awaitable<std::error_code>;
  auto initializeQueueIndex() -> boost::asio::awaitable<std::error_code>;
  auto initializeLogicalDevice() -> boost::asio::awaitable<std::error_code>;
  auto initializeDynamicDispatcher() -> boost::asio::awaitable<std::error_code>;
  auto initializeAllocator() -> boost::asio::awaitable<std::error_code>;
  auto initializeDescriptorSetAllocator() -> boost::asio::awaitable<std::error_code>;
  auto initializeQueues() -> boost::asio::awaitable<std::error_code>;
  auto initializeSynchronization() -> boost::asio::awaitable<std::error_code>;
  auto initializeSurfaceFormat() -> boost::asio::awaitable<std::error_code>;
  auto initializePrimaryRenderPass() -> boost::asio::awaitable<std::error_code>;
  auto initializeSwapchain() -> boost::asio::awaitable<std::error_code>;
  auto initializePipelineCache() -> boost::asio::awaitable<std::error_code>;
  auto initializeCommandPool() -> boost::asio::awaitable<std::error_code>;
  auto initializeCommandBuffers() -> boost::asio::awaitable<std::error_code>;

  auto updateSwapchain() -> boost::asio::awaitable<std::error_code>;
  void cleanupSwapchain();
  void cleanupRenderPass();

  auto sync() -> boost::asio::awaitable<void>;
};

}  // namespace gravity