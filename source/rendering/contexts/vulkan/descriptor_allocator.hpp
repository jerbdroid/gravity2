#pragma once

#include "vulkan/vulkan_raii.hpp"

#include <memory>

namespace gravity {

class DescriptorAllocatorPool {
 public:
  class DescriptorAllocatorHandle {
   public:
    DescriptorAllocatorHandle(DescriptorAllocatorPool* owning_pool,
                              vk::DescriptorPool descriptor_pool, size_t pool_index_)
        : owning_pool_{owning_pool}, descriptor_pool_{descriptor_pool}, pool_index_{pool_index_} {}

    DescriptorAllocatorHandle(const DescriptorAllocatorHandle&) = delete;
    auto operator=(const DescriptorAllocatorHandle&) -> DescriptorAllocatorHandle& = delete;

    ~DescriptorAllocatorHandle();
    DescriptorAllocatorHandle(DescriptorAllocatorHandle&& other) noexcept;
    auto operator=(DescriptorAllocatorHandle&& other) noexcept -> DescriptorAllocatorHandle&;

    // return this handle to the pool. Will make this handle orphaned
    void release();

    // allocate new descriptor. handle has to be valid
    // returns true if allocation succeeded, and false if it didn't
    // will mutate the handle if it requires a new vkDescriptorPool
    auto allocate(const vk::DescriptorSetLayout& layout, vk::DescriptorSet& built_set,
                  bool was_called) -> bool;

    [[nodiscard]] auto index() const -> size_t { return pool_index_; }

    [[nodiscard]] auto pool() const -> vk::DescriptorPool { return descriptor_pool_; }

   private:
    DescriptorAllocatorPool* owning_pool_{nullptr};
    vk::DescriptorPool descriptor_pool_{};
    size_t pool_index_{};
  };

  virtual ~DescriptorAllocatorPool() = default;
  DescriptorAllocatorPool() = default;
  DescriptorAllocatorPool(const DescriptorAllocatorPool&) = delete;
  DescriptorAllocatorPool(const DescriptorAllocatorPool&&) = delete;
  auto operator=(const DescriptorAllocatorPool&) -> DescriptorAllocatorPool& = delete;
  auto operator=(const DescriptorAllocatorPool&&) -> DescriptorAllocatorPool& = delete;

  static auto create(const vk::Device& device, int frames = 3)
      -> std::unique_ptr<DescriptorAllocatorPool>;

  // not thread safe
  // switches default allocators to the next frame. When frames loop it will
  // reset the descriptors of that frame
  virtual void flip() = 0;

  // not thread safe
  // override the pool size for a specific descriptor type. This will be used
  // new pools are allocated
  virtual void setPoolSizeMultiplier(vk::DescriptorType type, float multiplier) = 0;

  // thread safe, uses lock
  // get handle to use when allocating descriptors
  virtual auto getAllocator() -> DescriptorAllocatorHandle = 0;
};
}  // namespace gravity
