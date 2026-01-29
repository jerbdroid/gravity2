#pragma once

#include "source/common/scheduler/scheduler.hpp"

#include <expected>
#include <unordered_map>
#include <vector>

namespace gravity {

struct ShaderSourceResource {
  std::vector<uint32_t> spirv_;
  HashType hash_;
};

struct ShaderSourceResourceDescriptor {
  std::string path;
};

struct ShaderSourceResourceSlot {
  ShaderSourceResourceDescriptor key_;
  std::unique_ptr<ShaderSourceResource> shader_resource_;

  size_t index_ = 0;
  size_t generation_ = 0;

  size_t reference_counter_ = 0;

  bool loading_ = false;
  bool loaded_ = false;
};

struct ShaderSourceResourceHandle {
  size_t index_;
  size_t generation_;
};

struct ShaderResourceHash {
  auto operator()(const ShaderSourceResourceDescriptor& key) const -> HashType {
    return std::hash<std::string>()(key.path);
  }
};

class ResourceManager {
 public:
  enum class StrandLanes : uint8_t { Shaders, _Count };
  using StrandGroup = StrandGroup<ResourceManager>;

  ResourceManager(StrandGroup strands);

  auto acquireShaderSourceResource(
      const ShaderSourceResourceDescriptor& shader_resource_description)
      -> boost::asio::awaitable<std::expected<ShaderSourceResourceHandle, std::error_code>>;
  auto releaseShaderSourceResource(ShaderSourceResourceHandle shader_resource_handle)
      -> boost::asio::awaitable<void>;
  [[nodiscard]] auto getShader(ShaderSourceResourceHandle shader_resource_handle) const
      -> const ShaderSourceResource&;

 private:
  StrandGroup strands_;

  // Shader Resource
  std::vector<ShaderSourceResourceSlot> shader_source_resources_;
  std::unordered_map<ShaderSourceResourceDescriptor, ShaderSourceResourceHandle, ShaderResourceHash>
      shader_source_resource_cache_;
  std::vector<size_t> shaders_source_resource_free_list_;

  auto doAcquireShaderSourceResource(const ShaderSourceResourceDescriptor& shader_key)
      -> boost::asio::awaitable<std::expected<ShaderSourceResourceHandle, std::error_code>>;
  auto doReleaseShaderSourceResource(ShaderSourceResourceHandle shader_resource_handle)
      -> boost::asio::awaitable<void>;
};

}  // namespace gravity