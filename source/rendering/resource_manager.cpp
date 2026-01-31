#include "resource_manager.hpp"

#include "source/common/logging/logger.hpp"

#include "boost/asio/use_awaitable.hpp"
#include "magic_enum.hpp"

#include <cassert>
#include <expected>

#undef GRAVITY_MODULE_NAME
#define GRAVITY_MODULE_NAME "resource_manager"

namespace asio = boost::asio;

namespace gravity {

auto operator==(const ResourceDescriptor& descriptor, const ResourceDescriptor& other_description)
    -> bool {
  return descriptor.path_ == other_description.path_;
}

auto toIndex(ResourceType type) -> size_t {
  switch (type) {
    case ResourceType::Shader:
      return 0;
    case ResourceType::Image:
      return 1;
    case ResourceType::Mesh:
      return 2;
    case ResourceType::Material:
      return 3;
  }

  std::unreachable();
}

ResourceLease::ResourceLease(ResourceManager* resource_manager, ResourceHandle handle)
    : resource_manager_{ resource_manager }, handle_{ handle } {}

ResourceLease::ResourceLease(ResourceLease&& other) noexcept {
  std::swap(handle_, other.handle_);
  std::swap(resource_manager_, other.resource_manager_);
}

auto ResourceLease::operator=(ResourceLease&& other) noexcept -> ResourceLease& {
  if (this != &other) {
    release();

    std::swap(handle_, other.handle_);
    std::swap(resource_manager_, other.resource_manager_);
  }
  return *this;
}

ResourceLease::~ResourceLease() {
  release();
}

void ResourceLease::release() {
  if (resource_manager_ != nullptr) {
    resource_manager_->releaseResource(handle_);
  }
  resource_manager_ = nullptr;
}

ResourceManager::ResourceManager(StrandGroup strands) : strands_{ std::move(strands) } {}

auto ResourceManager::acquireResource(const ResourceDescriptor& descriptor)
    -> asio::awaitable<std::expected<ResourceLease, std::error_code>> {
  assert(static_cast<size_t>(descriptor.type_) != 0);
  co_return co_await asio::co_spawn(
      strands_.getStrand(descriptor.type_), doAcquireResource(descriptor), asio::use_awaitable);
}

void ResourceManager::releaseResource(ResourceHandle handle) {
  asio::co_spawn(
      strands_.getStrand(handle.type_),
      [this, handle]() -> asio::awaitable<void> {
        auto& resource_storage = contexts_[toIndex(handle.type_)].resources_;
        auto& cache = contexts_[toIndex(handle.type_)].cache_;
        auto& free_list = contexts_[toIndex(handle.type_)].free_list_;

        auto& resource_slot = resource_storage[handle.index_];

        assert(resource_slot.generation_ == handle.generation_);
        assert(resource_slot.reference_counter_ > 0);

        if (--resource_slot.reference_counter_ == 0) {
          LOG_DEBUG("releasing shader resource; path: {}", resource_slot.descriptor_.path_);

          cache.erase(resource_slot.descriptor_);

          resource_slot.generation_++;
          resource_slot.resource_.reset();
          resource_slot.loaded_ = false;

          free_list.emplace_back(handle.index_);
        }

        co_return;
      },
      asio::detached);
}

auto ResourceManager::getResource(const ResourceLease& lease) const
    -> boost::asio::awaitable<const Resource*> {
  co_return co_await asio::co_spawn(
      strands_.getStrand(lease.handle_.type_),
      [this, handle = lease.handle_]() -> asio::awaitable<const Resource*> {
        const auto& resource_storage = contexts_[toIndex(handle.type_)].resources_;
        co_return resource_storage[handle.index_].resource_.get();
      },
      asio::use_awaitable);
}

/*
 *  Private
 */

auto ResourceManager::doAcquireResource(const ResourceDescriptor& descriptor)
    -> asio::awaitable<std::expected<ResourceLease, std::error_code>> {
  constexpr std::chrono::microseconds WaitDuration{ 50 };

  auto& resource_storage{ contexts_[toIndex(descriptor.type_)].resources_ };
  auto& cache{ contexts_[toIndex(descriptor.type_)].cache_ };
  auto& free_list{ contexts_[toIndex(descriptor.type_)].free_list_ };

  auto executor = co_await asio::this_coro::executor;

  LOG_DEBUG("loading {}; path: {}", magic_enum::enum_name(descriptor.type_), descriptor.path_);

  if (auto iterator = cache.find(descriptor); iterator != cache.end()) {
    auto& cached_handle = iterator->second;
    auto* resource_slot = &resource_storage[cached_handle.index_];

    assert(cached_handle.generation_ == resource_slot->generation_);
    resource_slot->reference_counter_++;

    while (resource_slot->loading_) {
      co_await asio::steady_timer(executor, WaitDuration).async_wait(asio::use_awaitable);

      // we need to reload resource slot because the vector may have resized while we were awaiting
      // async read.
      auto* resource_slot = &resource_storage[cached_handle.index_];
    }

    assert(resource_slot->loaded_);

    if (!resource_slot->loaded_) {
      LOG_DEBUG(
          "cached {} resource not loaded; path {}", magic_enum::enum_name(descriptor.type_),
          descriptor.path_);

      releaseResource(cached_handle);
      co_return std::unexpected(Error::InternalError);
    }

    LOG_DEBUG(
        "loading {} resource using cached value; path {}", magic_enum::enum_name(descriptor.type_),
        descriptor.path_);
    co_return ResourceLease{ this, cached_handle };
  }

  auto file = std::make_shared<asio::stream_file>(
      strands_.getExecutor(), descriptor.path_, asio::stream_file::read_only);
  auto dynamic_buffer = std::make_shared<asio::streambuf>();

  size_t slot_index = 0;

  if (free_list.empty()) {
    resource_storage.emplace_back();
    slot_index = resource_storage.size() - 1;
  } else {
    slot_index = free_list.back();
    free_list.pop_back();
  }

  auto* resource_slot = &resource_storage[slot_index];
  resource_slot->reference_counter_++;
  resource_slot->index_ = slot_index;
  resource_slot->loading_ = true;

  auto [iterator, inserted] = cache.emplace(
      descriptor, ResourceHandle{ .type_ = descriptor.type_,
                                  .index_ = slot_index,
                                  .generation_ = resource_slot->generation_ });

  // It's not possible for another strand to create a same cache entry. Reused slots would have been
  // removed from cache.
  assert(inserted);

  auto& handle = iterator->second;

  auto [error_code, bytes] = co_await asio::async_read(
      *file, *dynamic_buffer, asio::transfer_all(), asio::as_tuple(asio::use_awaitable));

  // we need to reload resource slot because the vector may have resized while we were awaiting
  // async read.
  resource_slot = &resource_storage[slot_index];

  if (error_code == asio::error::eof || !error_code) {
    std::istream input_stream(dynamic_buffer.get());
    std::vector<char> buffer(
        (std::istreambuf_iterator<char>(input_stream)), std::istreambuf_iterator<char>());

    auto resource = std::make_unique<Resource>();
    resource->data_.resize(buffer.size());
    std::memcpy(resource->data_.data(), buffer.data(), buffer.size());
    resource->hash_ = hash(resource->data_);

    resource_slot->resource_ = std::move(resource);
    resource_slot->loaded_ = true;

    LOG_DEBUG(
        "loading {} resource was successful; path: {}, index: {}, generation: {}",
        magic_enum::enum_name(descriptor.type_), descriptor.path_, slot_index,
        resource_slot->generation_);

  } else {
    LOG_ERROR(
        "failed to read {} resource; path: {}; error: {}", magic_enum::enum_name(descriptor.type_),
        descriptor.path_, error_code.message());
    resource_slot->loading_ = false;
    releaseResource(handle);

    co_return std::unexpected(Error::InternalError);
  }

  resource_slot->descriptor_ = descriptor;
  resource_slot->loading_ = false;

  co_return ResourceLease{ this, handle };
}

}  // namespace gravity