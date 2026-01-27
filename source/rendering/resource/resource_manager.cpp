#include "resource_manager.hpp"

#include "source/common/logging/logger.hpp"

#include "boost/asio/use_awaitable.hpp"
#include "gsl/gsl"

#include <cassert>
#include <expected>

#define GRAVITY_MODULE_NAME "resource_manager"

namespace gravity {

auto operator==(
    const ShaderResourceDescription& description,
    const ShaderResourceDescription& other_description) -> bool {
  return description.path == other_description.path;
}

ResourceManager::ResourceManager(StrandGroup strands) : strands_{ std::move(strands) } {}

auto ResourceManager::acquireShaderResource(
    const ShaderResourceDescription& shader_resource_description)
    -> boost::asio::awaitable<std::expected<ShaderResourceHandle, std::error_code>> {

  co_return co_await boost::asio::co_spawn(
      strands_.getStrand(StrandLanes::Shaders),
      doAcquireShaderResource(shader_resource_description), boost::asio::use_awaitable);
}

auto ResourceManager::releaseShaderResource(ShaderResourceHandle shader_resource_handle)
    -> boost::asio::awaitable<void> {

  co_return co_await boost::asio::co_spawn(
      strands_.getStrand(StrandLanes::Shaders), doReleaseShaderResource(shader_resource_handle),
      boost::asio::use_awaitable);
}

auto ResourceManager::getShader(ShaderResourceHandle shader_resource_handle) const
    -> const ShaderResource& {

  return *shader_resources_[shader_resource_handle.index_].shader_resource_;
}

auto ResourceManager::doAcquireShaderResource(const ShaderResourceDescription& shader_key)
    -> boost::asio::awaitable<std::expected<ShaderResourceHandle, std::error_code>> {

  constexpr std::chrono::microseconds WaitDuration{ 50 };
  auto executor = co_await boost::asio::this_coro::executor;

  LOG_DEBUG("loading shader; path: {}", shader_key.path);

  if (auto iterator = shader_resource_cache_.find(shader_key);
      iterator != shader_resource_cache_.end()) {
    assert(iterator->second.generation_ == shader_resources_[iterator->second.index_].generation_);
    shader_resources_[iterator->second.index_].reference_counter_++;

    while (shader_resources_[iterator->second.index_].loading_) {
      co_await boost::asio::steady_timer(executor, WaitDuration)
          .async_wait(boost::asio::use_awaitable);
    }

    assert(shader_resources_[iterator->second.index_].loaded_);
    LOG_DEBUG("loading shader using cached value; path {}", shader_key.path);
    co_return iterator->second;
  }

  auto file = std::make_shared<boost::asio::stream_file>(
      strands_.getExecutor(), shader_key.path, boost::asio::stream_file::read_only);
  auto dynamic_buffer = std::make_shared<boost::asio::streambuf>();

  size_t shader_resource_slot_index = 0;

  if (shaders_resource_free_list_.empty()) {
    shader_resources_.emplace_back();
    shader_resource_slot_index = shader_resources_.size() - 1;
    shader_resources_[shader_resource_slot_index].index_ = shader_resource_slot_index;
  } else {
    shader_resource_slot_index = shaders_resource_free_list_.back();
    shaders_resource_free_list_.pop_back();
  }

  auto [iter, inserted] = shader_resource_cache_.emplace(
      shader_key, ShaderResourceHandle{ .index_ = shader_resource_slot_index, .generation_ = 0 });

  auto guard = gsl::finally([&] {
    if (shader_resources_[shader_resource_slot_index].loading_) {
      LOG_DEBUG("loading shader resource aborted");
      shader_resource_cache_.erase(shader_key);
      shaders_resource_free_list_.push_back(shader_resource_slot_index);
    }
  });

  shader_resources_[shader_resource_slot_index].loading_ = true;

  auto [error_code, bytes] = co_await boost::asio::async_read(
      *file, *dynamic_buffer, boost::asio::transfer_all(),
      boost::asio::as_tuple(boost::asio::use_awaitable));

  if (error_code == boost::asio::error::eof || !error_code) {
    std::istream input_stream(dynamic_buffer.get());
    std::vector<char> buffer(
        (std::istreambuf_iterator<char>(input_stream)), std::istreambuf_iterator<char>());

    if (buffer.size() % 4 != 0) {
      LOG_ERROR("Shader file size is not multiple of 4 bytes");
      co_return std::unexpected(Error::InternalError);
    }

    auto shader_resource = std::make_unique<ShaderResource>();
    shader_resource->spirv_.resize(buffer.size() / 4);
    std::memcpy(shader_resource->spirv_.data(), buffer.data(), buffer.size());
    shader_resource->hash_ = hash(shader_resource->spirv_);

    shader_resources_[shader_resource_slot_index].shader_resource_ = std::move(shader_resource);
    shader_resources_[shader_resource_slot_index].loaded_ = true;

    LOG_DEBUG(
        "loading shader successful; index: {}, generation: {}", shader_resource_slot_index,
        shader_resources_[shader_resource_slot_index].generation_);

  } else {
    LOG_ERROR("shader module read error: {}", error_code.message());
    co_return std::unexpected(Error::InternalError);
  }

  shader_resources_[shader_resource_slot_index].key_ = shader_key;
  shader_resources_[shader_resource_slot_index].loading_ = false;
  shader_resources_[shader_resource_slot_index].reference_counter_++;

  co_return iter->second;
}

auto ResourceManager::doReleaseShaderResource(ShaderResourceHandle shader_resource_handle)
    -> boost::asio::awaitable<void> {

  assert(
      shader_resources_[shader_resource_handle.index_].generation_ ==
      shader_resource_handle.generation_);
  assert(shader_resources_[shader_resource_handle.index_].reference_counter_ > 0);

  if (--shader_resources_[shader_resource_handle.index_].reference_counter_ == 0) {
    LOG_DEBUG(
        "releasing shader resource; path: {}",
        shader_resources_[shader_resource_handle.index_].key_.path);

    shader_resource_cache_.erase(shader_resources_[shader_resource_handle.index_].key_);

    shader_resources_[shader_resource_handle.index_].generation_++;
    shader_resources_[shader_resource_handle.index_].shader_resource_.reset();
    shader_resources_[shader_resource_handle.index_].loaded_ = false;
  }

  co_return;
}

}  // namespace gravity