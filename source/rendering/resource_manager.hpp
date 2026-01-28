#pragma once

#include "source/common/scheduler/scheduler.hpp"

#include <expected>
#include <unordered_map>
#include <vector>

namespace gravity {

struct ShaderResource {
  std::vector<uint32_t> spirv_;
  HashType hash_;
};

struct ShaderResourceDescriptor {
  std::string path;
};

struct ShaderResourceSlot {
  ShaderResourceDescriptor key_;
  std::unique_ptr<ShaderResource> shader_resource_;

  size_t index_ = 0;
  size_t generation_ = 0;

  size_t reference_counter_ = 0;

  bool loading_ = false;
  bool loaded_ = false;
};

struct ShaderResourceHandle {
  size_t index_;
  size_t generation_;
};

struct ShaderResourceHash {
  auto operator()(const ShaderResourceDescriptor& key) const -> HashType {
    return std::hash<std::string>()(key.path);
  }
};

class ResourceManager {
 public:
  enum class StrandLanes : uint8_t { Shaders, _Count };
  using StrandGroup = StrandGroup<ResourceManager>;

  ResourceManager(StrandGroup strands);

  auto acquireShaderResource(const ShaderResourceDescriptor& shader_resource_description)
      -> boost::asio::awaitable<std::expected<ShaderResourceHandle, std::error_code>>;
  auto releaseShaderResource(ShaderResourceHandle shader_resource_handle)
      -> boost::asio::awaitable<void>;
  [[nodiscard]] auto getShader(ShaderResourceHandle shader_resource_handle) const
      -> const ShaderResource&;

 private:
  StrandGroup strands_;

  // Shader Resource
  std::vector<ShaderResourceSlot> shader_resources_;
  std::unordered_map<ShaderResourceDescriptor, ShaderResourceHandle, ShaderResourceHash>
      shader_resource_cache_;
  std::vector<size_t> shaders_resource_free_list_;

  auto doAcquireShaderResource(const ShaderResourceDescriptor& shader_key)
      -> boost::asio::awaitable<std::expected<ShaderResourceHandle, std::error_code>>;
  auto doReleaseShaderResource(ShaderResourceHandle shader_resource_handle)
      -> boost::asio::awaitable<void>;
};

}  // namespace gravity