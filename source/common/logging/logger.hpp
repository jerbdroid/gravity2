#pragma once

#include "source/common/error.hpp"

#ifdef NDEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_WARN
#else
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif

#include "spdlog/async_logger.h"
#include "spdlog/spdlog.h"

namespace gravity {

auto setupAsyncLogger() -> std::error_code;

}  // namespace gravity

// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
#define LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
// NOLINTEND(cppcoreguidelines-macro-usage)