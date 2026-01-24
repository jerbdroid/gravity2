#include "logger.hpp"

// #include "config.hpp"

#include "spdlog/async.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace gravity {

auto setupAsyncLogger() -> std::error_code {
  static std::shared_ptr<spdlog::logger> logger;

  if (logger != nullptr) {
    LOG_WARN("async logger already installed");
    return Error::AlreadyExistsError;
  }

  // const auto& logger_config{LoggerConfig::getInstance()};

  spdlog::sink_ptr sink{nullptr};

  // if (logger_config.useFile()) {
  //   sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
  //       "logs/gravity.log", logger_config.maxFileSize(),
  //       logger_config.maxFileCount());
  // } else {
  sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  // }

  // if (logger_config.useSync()) {
  spdlog::init_thread_pool(16384, // logger_config.asyncQueueSize(),
                           1      // logger_config.asyncBackingThreads()
  );

  logger = std::make_shared<spdlog::async_logger>(
      "default", sink, spdlog::thread_pool(),
      spdlog::async_overflow_policy::discard_new);
  // } else {
  //   logger = std::make_shared<spdlog::logger>("default", sink);
  // }

  spdlog::set_default_logger(logger);

  // spdlog::set_level(spdlog::level::from_str(logger_config.logLevel()));

  spdlog::set_level(spdlog::level::trace);

  spdlog::set_pattern("[%^%l%$][%t][%s:%#] %v");

  return Error::OK;
}

} // namespace gravity