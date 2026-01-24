#pragma once

#include "api/config/bootstrap.pb.h"

namespace gravity {

class LoggerConfig {
  static constexpr size_t DefaultLogQueueSize{16384};
  static constexpr uint32_t DefaultMaxFileSize{1024 * 1024 * 10};
  static constexpr uint32_t DefaultMaxFileCount{3};

 public:
  static auto getInstance() -> const LoggerConfig&;

  [[nodiscard]] auto logLevel() const -> const std::string&;
  [[nodiscard]] auto useSync() const -> bool;
  [[nodiscard]] auto useFile() const -> bool;
  [[nodiscard]] auto asyncQueueSize() const -> size_t;
  [[nodiscard]] auto asyncBackingThreads() const -> size_t;
  [[nodiscard]] auto maxFileSize() const -> uint32_t;
  [[nodiscard]] auto maxFileCount() const -> uint32_t;

 private:
  explicit LoggerConfig(const gravity::config::Logger& raw_logger);

  std::string log_level_;
  bool use_async_;
  bool use_file_;
  size_t async_queue_size_;
  size_t async_backing_threads_;

  uint32_t max_file_size_;
  uint32_t max_file_count_;
};

}  // namespace gravity