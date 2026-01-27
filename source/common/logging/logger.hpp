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
auto getOrCreateLogger(const std::string& name) -> std::shared_ptr<spdlog::logger>;

}  // namespace gravity

// Each .cpp file can define its own GRAVITY_MODULE_NAME before including this header,
// or you can default it to "default"
#ifndef GRAVITY_MODULE_NAME
#define GRAVITY_MODULE_NAME "default"
#endif

// Internal helper to fetch the correct logger for the current module
#define GET_MODULE_LOGGER() gravity::getOrCreateLogger(GRAVITY_MODULE_NAME)

// Updated Macros using SPDLOG_LOGGER_CALL for source location support (%s:%#)
#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(GET_MODULE_LOGGER(), __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(GET_MODULE_LOGGER(), __VA_ARGS__)
#define LOG_INFO(...) SPDLOG_LOGGER_INFO(GET_MODULE_LOGGER(), __VA_ARGS__)
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(GET_MODULE_LOGGER(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(GET_MODULE_LOGGER(), __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(GET_MODULE_LOGGER(), __VA_ARGS__)
