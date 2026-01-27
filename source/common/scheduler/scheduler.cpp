#include "scheduler.hpp"

#include "source/common/logging/logger.hpp"

#define GRAVITY_MODULE_NAME "scheduler"

namespace gravity {

Worker::Worker(size_t threads, std::string_view name) : io_guard_(io_context_.get_executor()) {
  for (size_t i = 0; i < threads; ++i) {
    thread_.emplace_back([name = absl::StrCat(name, "-", i), this] {
      LOG_TRACE("{} run() entered", name);
      io_context_.run();
      LOG_TRACE("{} run() exited", name);
    });
    setThreadName(thread_.back(), absl::StrCat(name, "-", i));
  }
}

Worker::~Worker() {
  io_guard_.reset();
  for (auto& thread : thread_) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

}  // namespace gravity