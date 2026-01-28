#pragma once

#include <cstdint>
#include "source/common/scheduler/scheduler.hpp"
#include "source/rendering/device/rendering_device.hpp"
#include "source/rendering/resource/resource_manager.hpp"

namespace gravity {

struct MaterialDescription {
  std::string vertex_shader_path_;
  std::string fragment_shader_path_;
};

class RenderingServer {
 public:
  enum class StrandLanes : uint8_t { Main, _Count };
  using StrandGroup = StrandGroup<RenderingServer>;

  ~RenderingServer() = default;

  RenderingServer(Scheduler& scheduler, RenderingDevice& device)
      : device_{ device },
        strands_{ scheduler.makeStrands<RenderingServer>() },
        resources_{ scheduler.makeStrands<ResourceManager>() } {}

  auto draw() -> boost::asio::awaitable<void>;

  auto loadShaderModule(std::string path) -> boost::asio::awaitable<void>;

 private:
  RenderingDevice& device_;

  StrandGroup strands_;
  ResourceManager resources_;
};

}  // namespace gravity