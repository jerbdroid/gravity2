#pragma once

#include <system_error>

namespace gravity {

enum class Error : std::uint8_t {
  OK,
  InternalError,
  AlreadyExistsError,
  InvalidArgumentError,
  NotFoundError,
  UnavailableError,
  UnimplementedError,
  AbortedError,
  FailedPreconditionError,
  FeatureNotSupported,
};

auto ErrorCategory() -> const std::error_category&;
auto make_error_code(Error error_code) -> std::error_code;

}  // namespace gravity

namespace std {

template <>
struct is_error_code_enum<gravity::Error> : true_type {};

}  // namespace std