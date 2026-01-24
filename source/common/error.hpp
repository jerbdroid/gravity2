#pragma once

#include <system_error>

namespace gravity {

enum class Error : std::uint8_t {
  OK = 0,
  InternalError = 1,
  AlreadyExistsError = 2,
  InvalidArgumentError = 3,
  NotFoundError = 4,
  UnavailableError = 5,
  UnimplementedError = 6,
  AbortedError = 7,
  FailedPreconditionError = 8
};

auto ErrorCategory() -> const std::error_category&;
auto make_error_code(Error error_code) -> std::error_code;

}  // namespace gravity

namespace std {

template <>
struct is_error_code_enum<gravity::Error> : true_type {};

}  // namespace std