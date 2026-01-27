#pragma once

#include "source/common/scheduler/scheduler.hpp"
#include "source/rendering/common/rendering_api.hpp"

#include <expected>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gravity {

struct ShaderResource {
  std::vector<uint32_t> spirv_;
};

struct ShaderKey {
  std::string path;
  ShaderStage stage;
};

struct ShaderResourceSlot {
  ShaderKey key_;
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

struct ShaderKeyHash {
  auto operator()(const ShaderKey& key) const -> size_t {
    return std::hash<std::string>()(key.path) ^ (static_cast<size_t>(key.stage) << 1);
  }
};

class ResourceManager {
 public:
  enum class StrandLanes : uint8_t { Shaders, _Count };
  using StrandGroup = StrandGroup<ResourceManager>;

  ResourceManager(StrandGroup strands);

  auto acquireShader(const ShaderKey& shader_key)
      -> boost::asio::awaitable<std::expected<ShaderResourceHandle, std::error_code>>;
  auto releaseShader(ShaderResourceHandle resource_handle) -> boost::asio::awaitable<void>;
  [[nodiscard]] auto getShader(ShaderResourceHandle resource_handle) const -> const ShaderResource&;

 private:
  StrandGroup strands_;

  // Shader Resource
  std::vector<ShaderResourceSlot> shaders_resource_;
  std::unordered_map<ShaderKey, ShaderResourceHandle, ShaderKeyHash> shader_resource_cache_;
  std::vector<size_t> shaders_resource_free_list_;

  auto doAcquireShader(const ShaderKey& shader_key)
      -> boost::asio::awaitable<std::expected<ShaderResourceHandle, std::error_code>>;
  auto doReleaseShader(ShaderResourceHandle resource_handle) -> boost::asio::awaitable<void>;
};

}  // namespace gravity