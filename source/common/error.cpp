#include "error.hpp"

namespace gravity {

class ErrorCategory : public std::error_category {
public:
  [[nodiscard]] auto name() const noexcept -> const char * override {
    return "Gravity Error";
  }

  [[nodiscard]] auto equivalent(const std::error_code &code,
                                int condition) const noexcept -> bool override {
    return *this == code.category() && code.value() == condition;
  }

  [[nodiscard]] auto message(int error_code) const -> std::string override {
    switch (static_cast<Error>(error_code)) {
    case Error::OK:
      return "Success";
    case Error::InternalError:
      return "Internal Error";
    case Error::AlreadyExistsError:
      return "Already Exists Error";
    default:
      return "Unknown Error";
    }
  }
};

auto ErrorCategory() -> const std::error_category & {
  static class ErrorCategory instance;
  return instance;
}

auto make_error_code(Error error_code) -> std::error_code {
  return {static_cast<int>(error_code), ErrorCategory()};
}

} // namespace gravity