#include "config.hpp"

#include "source/common/config/config.hpp"

namespace gravity {

auto LoggerConfig::getInstance() -> const LoggerConfig& {
  static LoggerConfig logger_config{gravity::Configuration::getInstance().loggerConfig()};

  return logger_config;
}

auto LoggerConfig::logLevel() const -> const std::string& {
  return log_level_;
}

auto LoggerConfig::useSync() const -> bool {
  return use_async_;
}

auto LoggerConfig::useFile() const -> bool {
  return use_file_;
}

auto LoggerConfig::asyncQueueSize() const -> size_t {
  return async_queue_size_;
}

auto LoggerConfig::asyncBackingThreads() const -> size_t {
  return async_backing_threads_;
}

auto LoggerConfig::maxFileSize() const -> uint32_t {
  return max_file_size_;
}

auto LoggerConfig::maxFileCount() const -> uint32_t {
  return max_file_count_;
}

LoggerConfig::LoggerConfig(const gravity::config::Logger& raw_logger)
    : log_level_{raw_logger.has_log_level() ? raw_logger.log_level() : "info"},
      use_async_{raw_logger.has_use_async() ? raw_logger.use_async() : true},
      use_file_{raw_logger.has_use_file() ? raw_logger.use_file() : false},
      async_queue_size_{raw_logger.has_async_queue_size() ? raw_logger.async_queue_size()
                                                          : DefaultLogQueueSize},
      async_backing_threads_{
          raw_logger.has_async_backing_threads() ? raw_logger.async_backing_threads() : 1},
      max_file_size_{raw_logger.has_max_file_size() ? raw_logger.max_file_size()
                                                    : DefaultMaxFileSize},
      max_file_count_{raw_logger.has_max_file_count() ? raw_logger.max_file_count()
                                                      : DefaultMaxFileCount} {}

}  // namespace gravity