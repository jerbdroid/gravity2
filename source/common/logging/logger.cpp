#include "logger.hpp"

// #include "config.hpp"

#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <mutex>

namespace gravity {

// Shared sinks and config
static std::shared_ptr<spdlog::sinks::sink> global_sink;
static bool is_initialized = false;

auto setupAsyncLogger() -> std::error_code {
  if (is_initialized) {
    return Error::AlreadyExistsError;
  }

  // 1. Initialize shared sink (Console/File)
  global_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

  // 2. Initialize global thread pool for async logging
  spdlog::init_thread_pool(16384, 1);

  // 3. Set global formatting pattern
  spdlog::set_pattern("[%^%l%$][%t][%n][%s:%#] %v");  // Added [%n] for module name

  is_initialized = true;
  return Error::OK;
}

static std::mutex logger_mutex;

// Helper to get or create a module-specific logger
auto getOrCreateLogger(const std::string& name) -> std::shared_ptr<spdlog::logger> {
  auto logger = spdlog::get(name);
  if (!logger) {
    std::lock_guard<std::mutex> lock(logger_mutex);
    auto logger = spdlog::get(name);
    if (!logger) {
      // Create new async logger using the shared thread pool and sink
      logger = std::make_shared<spdlog::async_logger>(
          name, global_sink, spdlog::thread_pool(), spdlog::async_overflow_policy::discard_new);

      // Set module-specific default level (can be overridden later)
      logger->set_level(spdlog::level::trace);
      spdlog::register_logger(logger);
    }
    return logger;
  }
  return logger;
}

}  // namespace gravity
