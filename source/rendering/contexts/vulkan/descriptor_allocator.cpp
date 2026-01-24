#include "descriptor_allocator.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <vector>

namespace gravity {

auto IsMemoryError(VkResult errorResult) -> bool {
  switch (errorResult) {
    case VK_ERROR_FRAGMENTED_POOL:
    case VK_ERROR_OUT_OF_POOL_MEMORY:
      return true;
    default:
      return false;
  }
}

struct DescriptorAllocator {
  VkDescriptorPool pool;
};

struct PoolStorage {
  std::vector<DescriptorAllocator> usable_allocators_;
  std::vector<DescriptorAllocator> full_allocators_;
};

struct PoolSize {
  vk::DescriptorType type;
  float multiplier;
};

struct PoolSizeList {
  std::vector<PoolSize> available_size_ = {{vk::DescriptorType::eSampler, 1.0F},
                                           {vk::DescriptorType::eCombinedImageSampler, 4.0F},
                                           {vk::DescriptorType::eSampledImage, 4.0F},
                                           {vk::DescriptorType::eStorageImage, 1.0F},
                                           {vk::DescriptorType::eUniformTexelBuffer, 1.0F},
                                           {vk::DescriptorType::eStorageTexelBuffer, 1.0F},
                                           {vk::DescriptorType::eUniformBuffer, 2.0F},
                                           {vk::DescriptorType::eStorageBuffer, 2.0F},
                                           {vk::DescriptorType::eUniformBufferDynamic, 1.0F},
                                           {vk::DescriptorType::eStorageBufferDynamic, 1.0F},
                                           {vk::DescriptorType::eInputAttachment, 1.0F}};
};

class DescriptorAllocatorPoolImpl : public DescriptorAllocatorPool {
 public:
  static constexpr uint32_t DefaultMaxSet{2000};

  ~DescriptorAllocatorPoolImpl() override;
  DescriptorAllocatorPoolImpl() = delete;

  DescriptorAllocatorPoolImpl(vk::Device device, size_t frames)
      : device_{device}, max_frames_{frames} {
    descriptor_pools_.reserve(frames);
    for (int i = 0; i < frames; i++) {
      descriptor_pools_.emplace_back(std::make_unique<PoolStorage>());
    }
  }

  DescriptorAllocatorPoolImpl(DescriptorAllocatorPoolImpl&) = delete;
  DescriptorAllocatorPoolImpl(DescriptorAllocatorPoolImpl&&) = delete;
  auto operator=(const DescriptorAllocatorPoolImpl&) -> DescriptorAllocatorPoolImpl& = delete;
  auto operator=(const DescriptorAllocatorPoolImpl&&) -> DescriptorAllocatorPoolImpl& = delete;

  void flip() override;
  void setPoolSizeMultiplier(vk::DescriptorType type, float multiplier) override;
  auto getAllocator() -> DescriptorAllocatorHandle override;

  void returnAllocator(DescriptorAllocatorHandle& handle, bool is_full);
  auto createPool(uint32_t max_set, vk::DescriptorPoolCreateFlags flags) -> vk::DescriptorPool;

  friend class DescriptorAllocatorPool;

 private:
  vk::Device device_{};
  PoolSizeList pool_size_;
  size_t frame_index_{};
  size_t max_frames_{};

  std::mutex mutex_;

  // zero is for static pool, next is for frame indexing
  std::vector<std::unique_ptr<PoolStorage>> descriptor_pools_;

  // fully cleared allocators
  std::vector<DescriptorAllocator> clear_allocators_;
};

auto gravity::DescriptorAllocatorPool::create(const vk::Device& device, int frames)
    -> std::unique_ptr<DescriptorAllocatorPool> {
  return std::make_unique<DescriptorAllocatorPoolImpl>(device, frames);
}

DescriptorAllocatorPool::DescriptorAllocatorHandle::~DescriptorAllocatorHandle() {
  auto* owning_pool_impl = dynamic_cast<DescriptorAllocatorPoolImpl*>(owning_pool_);

  if (owning_pool_impl != nullptr) {
    owning_pool_impl->returnAllocator(*this, false);
  }
}

DescriptorAllocatorPool::DescriptorAllocatorHandle::DescriptorAllocatorHandle(
    DescriptorAllocatorHandle&& other) noexcept {
  release();

  std::swap(descriptor_pool_, other.descriptor_pool_);
  std::swap(pool_index_, other.pool_index_);
  std::swap(owning_pool_, other.owning_pool_);
}

auto DescriptorAllocatorPool::DescriptorAllocatorHandle::operator=(
    DescriptorAllocatorHandle&& other) noexcept
    -> DescriptorAllocatorPool::DescriptorAllocatorHandle& {
  release();

  std::swap(descriptor_pool_, other.descriptor_pool_);
  std::swap(pool_index_, other.pool_index_);
  std::swap(owning_pool_, other.owning_pool_);

  return *this;
}

void DescriptorAllocatorPool::DescriptorAllocatorHandle::release() {
  auto* owning_pool_impl = dynamic_cast<DescriptorAllocatorPoolImpl*>(owning_pool_);

  // TODO(jerbdroid): Implement error handling, but consider using something other than dynamic cast
  // if(owning_pool_impl == nullptr) {
  //   return absl::InternalError(absl::string_view message)
  // }

  if (owning_pool_impl != nullptr) {
    owning_pool_impl->returnAllocator(*this, false);
  }
}

// NOLINTNEXTLINE(misc-no-recursion)
auto DescriptorAllocatorPool::DescriptorAllocatorHandle::allocate(
    const vk::DescriptorSetLayout& layout, vk::DescriptorSet& built_set, bool was_called) -> bool {
  auto* owning_pool_impl = dynamic_cast<DescriptorAllocatorPoolImpl*>(owning_pool_);

  // TODO(jerbdroid): Implement error handling, but consider using something other than dynamic cast
  // if(owning_pool_impl == nullptr) {
  //   return absl::InternalError(absl::string_view message)
  // }

  vk::DescriptorSetAllocateInfo descriptor_set_allocate_info{descriptor_pool_, 1, &layout};
  auto descriptor_set_allocate{
      owning_pool_impl->device_.allocateDescriptorSets(descriptor_set_allocate_info)};

  switch (descriptor_set_allocate.result) {
    case vk::Result::eErrorFragmentedPool:
    case vk::Result::eErrorOutOfPoolMemory:
      *this = owning_pool_impl->getAllocator();
      if (was_called) {
        return false;
      }
      return allocate(layout, built_set, true);
    case vk::Result::eSuccess:
      break;
    default:
      return false;
  }

  built_set = descriptor_set_allocate.value.front();

  return true;
}

auto DescriptorAllocatorPoolImpl::createPool(uint32_t max_set, vk::DescriptorPoolCreateFlags flags)
    -> vk::DescriptorPool {
  std::vector<vk::DescriptorPoolSize> descriptor_pool_sizes;
  descriptor_pool_sizes.reserve(pool_size_.available_size_.size());

  for (const auto& pool_size : pool_size_.available_size_) {
    descriptor_pool_sizes.emplace_back(
        pool_size.type, static_cast<uint32_t>(pool_size.multiplier * static_cast<float>(max_set)));
  }

  auto descriptor_pool_create{
      device_.createDescriptorPool({flags, max_set, descriptor_pool_sizes})};

  // TODO(jerbdroid): Add support of error handling
  // if(descriptor_pool_create.result != vk::Result::eSuccess) {
  //   return absl::InternalError("unable to create descriptor pool");
  // }

  return descriptor_pool_create.value;
}

DescriptorAllocatorPoolImpl::~DescriptorAllocatorPoolImpl() {
  std::for_each(clear_allocators_.begin(), clear_allocators_.end(),
                [this](const auto& allocator) { device_.destroyDescriptorPool(allocator.pool); });
  std::for_each(descriptor_pools_.begin(), descriptor_pools_.end(),
                [this](const std::unique_ptr<PoolStorage>& allocator) {
                  std::for_each(allocator->full_allocators_.begin(),
                                allocator->full_allocators_.end(), [this](const auto& allocator) {
                                  device_.destroyDescriptorPool(allocator.pool);
                                });
                  std::for_each(allocator->usable_allocators_.begin(),
                                allocator->usable_allocators_.end(), [this](const auto& allocator) {
                                  device_.destroyDescriptorPool(allocator.pool);
                                });
                });
}

void DescriptorAllocatorPoolImpl::flip() {
  frame_index_ = (frame_index_ + 1) % max_frames_;

  for (const auto& full_allocator : descriptor_pools_[frame_index_]->full_allocators_) {
    device_.resetDescriptorPool(full_allocator.pool);
    clear_allocators_.push_back(full_allocator);
  }

  for (const auto& usable_allocator : descriptor_pools_[frame_index_]->usable_allocators_) {
    device_.resetDescriptorPool(usable_allocator.pool);
    clear_allocators_.push_back(usable_allocator);
  }

  descriptor_pools_[frame_index_]->full_allocators_.clear();
  descriptor_pools_[frame_index_]->usable_allocators_.clear();
}

void DescriptorAllocatorPoolImpl::setPoolSizeMultiplier(vk::DescriptorType type, float multiplier) {
  for (auto& size : pool_size_.available_size_) {
    if (size.type == type) {
      size.multiplier = multiplier;
      return;
    }
  }

  pool_size_.available_size_.emplace_back(type, multiplier);
}

void DescriptorAllocatorPoolImpl::returnAllocator(DescriptorAllocatorHandle& handle, bool is_full) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (is_full) {
    descriptor_pools_[handle.index()]->full_allocators_.push_back(
        DescriptorAllocator{handle.pool()});
  } else {
    descriptor_pools_[handle.index()]->usable_allocators_.push_back(
        DescriptorAllocator{handle.pool()});
  }
}

auto DescriptorAllocatorPoolImpl::getAllocator()
    -> DescriptorAllocatorPool::DescriptorAllocatorHandle {
  std::lock_guard<std::mutex> lock(mutex_);

  bool found_allocator = false;

  DescriptorAllocator allocator{};
  // try reuse an allocated pool
  if (!clear_allocators_.empty()) {
    allocator = clear_allocators_.back();
    clear_allocators_.pop_back();
    found_allocator = true;
  } else {
    if (!descriptor_pools_[frame_index_]->usable_allocators_.empty()) {
      allocator = descriptor_pools_[frame_index_]->usable_allocators_.back();
      descriptor_pools_[frame_index_]->usable_allocators_.pop_back();
      found_allocator = true;
    }
  }
  // need a new pool
  if (!found_allocator) {
    // static pool has to be free-able
    vk::DescriptorPoolCreateFlags flags;
    if (frame_index_ == 0) {
      flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    }
    allocator.pool = createPool(DefaultMaxSet, flags);

    found_allocator = true;
  }

  return DescriptorAllocatorHandle{this, allocator.pool, frame_index_};
}
}  // namespace gravity