#pragma once

#include "descriptor_allocator.hpp"
#include "source/common/scheduler/scheduler.hpp"
#include "source/platform/window/window_context.hpp"
#include "source/rendering/device/rendering_device.hpp"

#include "vma/vk_mem_alloc.h"
#include "vulkan/vulkan_core.h"
#include "vulkan/vulkan_raii.hpp"

#include "vulkan/vulkan_handles.hpp"
#include "vulkan/vulkan_structs.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

namespace gravity {

struct DeviceFeaturesWithTimeline {
  vk::PhysicalDeviceFeatures core_features_;
  vk::PhysicalDeviceVulkan12Features vulkan_12_features_;
};

class VulkanRenderingDevice : public RenderingDevice {
 public:
  enum class StrandLanes : uint8_t { Initialize, Buffer, Sampler, Shader, Cleanup, _Count };
  using StrandGroup = StrandGroup<VulkanRenderingDevice>;

  ~VulkanRenderingDevice();
  VulkanRenderingDevice(WindowContext& window_context, StrandGroup strands);

  auto initialize() -> boost::asio::awaitable<std::error_code> override;
  auto prepareBuffers() -> boost::asio::awaitable<std::error_code>;
  auto swapBuffers() -> boost::asio::awaitable<std::error_code>;

  auto createBuffer(const BufferDescription& description)
      -> boost::asio::awaitable<std::expected<BufferHandle, std::error_code>> override;
  auto destroyBuffer(BufferHandle buffer_handle)
      -> boost::asio::awaitable<std::error_code> override;

  auto createImage(const ImageDescription& description)
      -> boost::asio::awaitable<std::expected<ImageHandle, std::error_code>> override;
  auto destroyImage(ImageHandle image_handle) -> boost::asio::awaitable<std::error_code> override;

  auto createSampler(const SamplerDescription& description)
      -> boost::asio::awaitable<std::expected<SamplerHandle, std::error_code>> override;
  auto destroySampler(SamplerHandle sampler_handle)
      -> boost::asio::awaitable<std::error_code> override;

  auto createShader(ShaderDescription description)
      -> boost::asio::awaitable<std::expected<ShaderHandle, std::error_code>> override;
  auto destroyShader(ShaderHandle shader_handle)
      -> boost::asio::awaitable<std::error_code> override;

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

  struct Buffer {
    VkBuffer buffer_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_info_ = {};
    VkDeviceSize size_ = 0;
  };

  struct PendingDestroy {
    size_t index_;
    size_t fence_value_;
  };

  struct BufferSlot {
    Buffer buffer_;
    size_t generation_ = 0;
    bool alive_ = true;
    size_t index_ = 0;
  };

  struct Image {
    VkImage image_ = VK_NULL_HANDLE;
    VkImageView image_view_ = VK_NULL_HANDLE;
    VmaAllocation allocation_ = VK_NULL_HANDLE;
    VmaAllocationInfo allocation_info_ = {};

    VkImageCreateInfo image_create_info_ = {};
    VkImageViewCreateInfo image_view_create_info_ = {};
  };

  struct ImageSlot {
    Image image_ = {};
    size_t generation_ = 0;
    size_t index_ = 0;
  };

  struct Sampler {
    vk::raii::Sampler sampler_;
    vk::SamplerCreateInfo sampler_create_info_;
  };

  struct SamplerSlot {
    Sampler sampler_;
    size_t index_ = 0;
  };

  struct Shader {
    vk::raii::ShaderModule module_;
    ShaderStage stage_ = ShaderStage::Unknown;
  };

  struct ShaderSlot {
    Shader shader_;
    uint32_t generation_ = 0;
    size_t index_ = 0;
  };

  WindowContext& window_context_;

  StrandGroup strands_;

  vk::raii::Context vk_context_;

  std::optional<vk::raii::Instance> instance_;

  // device
  DeviceFeaturesWithTimeline device_features_;
  vk::PhysicalDeviceLimits device_limits_;
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

  std::optional<vk::raii::Semaphore> timeline_semaphore_;
  size_t timeline_value_{ 0 };

  // cache
  std::optional<vk::raii::PipelineCache> pipeline_cache_;

  // descriptor allocator
  std::unique_ptr<DescriptorAllocatorPool> descriptor_allocator_static_;

  // memory
  VmaAllocator memory_allocator_;

  // dynamic loader for EXT
  vk::detail::DispatchLoaderDynamic dynamic_dispatcher_;

  // Buffers
  std::vector<BufferSlot> buffers_;
  std::vector<PendingDestroy> pending_destroy_buffers_;
  std::vector<size_t> buffer_free_list_;

  // Images
  std::vector<ImageSlot> images_;
  std::vector<PendingDestroy> pending_destroy_images_;
  std::vector<size_t> image_free_list_;

  // Samplers
  std::vector<SamplerSlot> samplers_;

  // Shader Modules
  std::vector<ShaderSlot> shader_modules_;
  std::vector<PendingDestroy> pending_destroy_shader_modules_;
  std::vector<size_t> shader_module_free_list_;

  std::unordered_set<std::string> enabled_instance_extension_names_;
  std::unordered_set<std::string> enabled_instance_layer_names_;
  std::unordered_set<std::string> enabled_device_extension_names_;

  auto doInitialize() -> boost::asio::awaitable<std::error_code>;
  auto doCreateBuffer(BufferDescription description)
      -> boost::asio::awaitable<std::expected<BufferHandle, std::error_code>>;
  auto doDestroyBuffer(BufferHandle buffer_handle) -> boost::asio::awaitable<std::error_code>;
  auto doCreateImage(ImageDescription description)
      -> boost::asio::awaitable<std::expected<ImageHandle, std::error_code>>;
  auto doDestroyImage(ImageHandle image_handle) -> boost::asio::awaitable<std::error_code>;
  auto doCreateSampler(SamplerDescription description)
      -> boost::asio::awaitable<std::expected<SamplerHandle, std::error_code>>;
  auto doDestroySampler(SamplerHandle image_handle) -> boost::asio::awaitable<std::error_code>;
  auto doCreateShader(ShaderDescription description)
      -> boost::asio::awaitable<std::expected<ShaderHandle, std::error_code>>;
  auto doDestroyShader(ShaderHandle shader_handle) -> boost::asio::awaitable<std::error_code>;

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

  void collectPendingDestroy();

  void sync();
};

}  // namespace gravity