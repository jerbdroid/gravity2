#include "resource_manager.hpp"

#include "source/common/logging/logger.hpp"

#include <boost/scope_exit.hpp>
#include "boost/asio/use_awaitable.hpp"

#include <cassert>
#include <expected>

#include <gsl/gsl>

#define GRAVITY_MODULE_NAME "resource_manager"

namespace gravity {

auto operator==(const ShaderKey& key, const ShaderKey& other_key) -> bool {
  return key.path == other_key.path && key.stage == other_key.stage;
}

ResourceManager::ResourceManager(StrandGroup strands) : strands_{ std::move(strands) } {}

auto ResourceManager::acquireShader(const ShaderKey& shader_key)
    -> boost::asio::awaitable<std::expected<ShaderResourceHandle, std::error_code>> {
  co_return co_await boost::asio::co_spawn(
      strands_.getStrand(StrandLanes::Shaders), doAcquireShader(shader_key),
      boost::asio::use_awaitable);
}

auto ResourceManager::releaseShader(ShaderResourceHandle resource_handle)
    -> boost::asio::awaitable<void> {

  co_return co_await boost::asio::co_spawn(
      strands_.getStrand(StrandLanes::Shaders), doReleaseShader(resource_handle),
      boost::asio::use_awaitable);
}

auto ResourceManager::getShader(ShaderResourceHandle resource_handle) const
    -> const ShaderResource& {
  return *shaders_resource_[resource_handle.index_].shader_resource_;
}

auto ResourceManager::doAcquireShader(const ShaderKey& shader_key)
    -> boost::asio::awaitable<std::expected<ShaderResourceHandle, std::error_code>> {
  constexpr std::chrono::microseconds WaitDuration{ 50 };
  auto executor = co_await boost::asio::this_coro::executor;

  LOG_DEBUG("loading shader; path: {}", shader_key.path);

  if (auto iterator = shader_resource_cache_.find(shader_key);
      iterator != shader_resource_cache_.end()) {
    assert(iterator->second.generation_ == shaders_resource_[iterator->second.index_].generation_);
    shaders_resource_[iterator->second.index_].reference_counter_++;

    while (shaders_resource_[iterator->second.index_].loading_) {
      co_await boost::asio::steady_timer(executor, WaitDuration)
          .async_wait(boost::asio::use_awaitable);
    }

    assert(shaders_resource_[iterator->second.index_].loaded_);
    LOG_DEBUG("loading shader using cached value; path {}", shader_key.path);
    co_return iterator->second;
  }

  auto file = std::make_shared<boost::asio::stream_file>(
      strands_.getExecutor(), shader_key.path, boost::asio::stream_file::read_only);
  auto dynamic_buffer = std::make_shared<boost::asio::streambuf>();

  size_t shader_resource_slot_index = 0;

  if (shaders_resource_free_list_.empty()) {
    shaders_resource_.emplace_back();
    shader_resource_slot_index = shaders_resource_.size() - 1;
    shaders_resource_[shader_resource_slot_index].index_ = shader_resource_slot_index;
  } else {
    shader_resource_slot_index = shaders_resource_free_list_.back();
    shaders_resource_free_list_.pop_back();
  }

  auto [iter, inserted] = shader_resource_cache_.emplace(
      shader_key, ShaderResourceHandle{ .index_ = shader_resource_slot_index, .generation_ = 0 });

  auto guard = gsl::finally([&] {
    if (shaders_resource_[shader_resource_slot_index].loading_) {
      LOG_DEBUG("loading shader ");
      shader_resource_cache_.erase(shader_key);
      shaders_resource_free_list_.push_back(shader_resource_slot_index);
    }
  });

  shaders_resource_[shader_resource_slot_index].loading_ = true;

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

    shaders_resource_[shader_resource_slot_index].shader_resource_ = std::move(shader_resource);
    shaders_resource_[shader_resource_slot_index].loaded_ = true;

    LOG_DEBUG(
        "loading shader successful; index: {}, generation: {}", shader_resource_slot_index,
        shaders_resource_[shader_resource_slot_index].generation_);

  } else {
    LOG_ERROR("shader module read error: {}", error_code.message());
    co_return std::unexpected(Error::InternalError);
  }

  shaders_resource_[shader_resource_slot_index].key_ = shader_key;
  shaders_resource_[shader_resource_slot_index].loading_ = false;
  shaders_resource_[shader_resource_slot_index].reference_counter_++;

  co_return iter->second;
}

auto ResourceManager::doReleaseShader(ShaderResourceHandle resource_handle)
    -> boost::asio::awaitable<void> {
  assert(shaders_resource_[resource_handle.index_].generation_ == resource_handle.generation_);
  assert(shaders_resource_[resource_handle.index_].reference_counter_ > 0);

  if (--shaders_resource_[resource_handle.index_].reference_counter_ == 0) {
    LOG_DEBUG(
        "releasing shader resource; path: {}", shaders_resource_[resource_handle.index_].key_.path);

    shader_resource_cache_.erase(shaders_resource_[resource_handle.index_].key_);

    shaders_resource_[resource_handle.index_].generation_++;
    shaders_resource_[resource_handle.index_].shader_resource_.reset();
    shaders_resource_[resource_handle.index_].loaded_ = false;
  }

  co_return;
}

}  // namespace gravity