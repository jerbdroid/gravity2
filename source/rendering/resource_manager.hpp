#pragma once

#include "source/common/scheduler/scheduler.hpp"

#include "boost/asio.hpp"
#include "magic_enum.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

namespace gravity {

class ResourceManager;

enum class ResourceType : uint8_t { Shader = 1, Image, Mesh, Material };

struct ResourceDescriptor {
  ResourceType type_;

  std::string path_;
};

struct ResourceHandle {
  ResourceType type_;

  size_t index_;
  size_t generation_;
};

struct Resource {
  std::vector<uint8_t> data_;
  HashType hash_;
};

struct ResourceSlot {
  ResourceDescriptor descriptor_;
  std::unique_ptr<Resource> resource_;

  size_t index_ = 0;
  size_t generation_ = 0;

  size_t reference_counter_ = 0;

  bool loading_ = false;
  bool loaded_ = false;
};

struct ResourceLease {
  ResourceManager* resource_manager_ = nullptr;
  ResourceHandle handle_ = {};
  ResourceLease(ResourceManager* resource_manager, ResourceHandle handle);

  ResourceLease() = default;

  ResourceLease(const ResourceLease& other) = delete;
  auto operator=(const ResourceLease& other) -> ResourceLease& = delete;

  ResourceLease(ResourceLease&& other) noexcept;
  auto operator=(ResourceLease&& other) noexcept -> ResourceLease&;

  ~ResourceLease();

 private:
  void release();
};

struct ResourceDescriptorHash {
  auto operator()(const ResourceDescriptor& key) const -> HashType {
    return std::hash<std::string>()(key.path_);
  }
};

class ResourceManager {
 public:
  using StrandLanes = ResourceType;
  using StrandGroup = StrandGroup<ResourceManager>;

  ResourceManager(StrandGroup strands);

  auto acquireResource(const ResourceDescriptor& resource_descriptor)
      -> boost::asio::awaitable<std::expected<ResourceLease, std::error_code>>;

  void releaseResource(ResourceHandle resource_handle);

  [[nodiscard]] auto getResource(const ResourceLease& lease) const
      -> boost::asio::awaitable<const Resource*>;

 private:
  using ResourceList = std::vector<ResourceSlot>;
  using ResourceCache =
      std::unordered_map<ResourceDescriptor, ResourceHandle, ResourceDescriptorHash>;
  using ResourceFreeList = std::vector<size_t>;

  struct ResourceContext {
    ResourceList resources_;
    ResourceCache cache_;
    ResourceFreeList free_list_;
  };

  StrandGroup strands_;

  std::array<ResourceContext, magic_enum::enum_count<ResourceType>()> contexts_;

  auto doAcquireResource(const ResourceDescriptor& descriptor)
      -> boost::asio::awaitable<std::expected<ResourceLease, std::error_code>>;
};

}  // namespace gravity