// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <boost/outcome/result.hpp>
#include <fmt/core.h>
#include <optional>
#include <system_error>

namespace noir {

class UserErrorCategory : public std::error_category {
public:
  const char* name() const noexcept override {
    return "user";
  }
  std::string message(int condition) const override {
    return "unspecified error";
  }
};

inline const std::error_category& user_category() {
  static UserErrorCategory category{};
  return category;
}

class Error {
public:
  constexpr Error() noexcept = default;

  // NOTE: Do not set default value for 2nd parameter (error_category),
  // it makes `noir::Error` constructible from int, and break implicit construction of Result<int> from value.
  constexpr Error(int ec, const std::error_category& ecat): value_(ec), category_(&ecat) {}

  template<typename T>
  requires std::is_convertible_v<T, std::error_code> // FIXME: clang-format 15 RequiresClausePosition: OwnLine
  constexpr Error(T&& err) {
    auto& ec = static_cast<const std::error_code&>(err);
    value_ = ec.value();
    category_ = &ec.category();
  }

  Error(std::string_view message): value_(-1), message_(message) {}

  template<typename... T>
  static constexpr auto format(fmt::format_string<T...> fmt, T&&... args) {
    return Error(fmt::vformat(fmt, fmt::make_format_args(args...)));
  }

  void assign(int ec, const std::error_category& ecat) noexcept {
    value_ = ec;
    category_ = &ecat;
  }

  void clear() noexcept {
    value_ = 0;
    category_ = &user_category();
  }

  int value() const noexcept {
    return value_;
  }

  const std::error_category& category() const noexcept {
    return *category_;
  }

  std::string message() const {
    if (message_) {
      return *message_;
    } else {
      return category().message(value());
    }
  }

  explicit operator bool() const noexcept {
    return value() != 0;
  }

  bool operator==(const Error& err) const& noexcept {
    return category() == err.category() && value() == err.value() && message_ == err.message_;
  }

  bool operator!=(const Error& err) const& noexcept {
    return !(*this == err);
  }

  bool operator<(const Error& err) const& noexcept {
    return category() < err.category() || (category() == err.category() && value() < err.value()) ||
      (category() == err.category() && value() == err.value() && message_ < err.message_);
  }

private:
  int value_ = 0;
  const std::error_category* category_ = &user_category();
  std::optional<std::string> message_{};
};

} // namespace noir

namespace std {

template<>
struct hash<noir::Error> {
  size_t operator()(const noir::Error& err) const noexcept {
    return static_cast<size_t>(err.value());
  }
};

} // namespace std

BOOST_OUTCOME_V2_NAMESPACE_BEGIN

namespace policy::detail {

template<>
inline decltype(auto) exception_ptr<noir::Error&>(noir::Error& err) {
  return std::make_exception_ptr(std::error_code(err.value(), err.category()));
}
template<>
inline decltype(auto) exception_ptr<const noir::Error&>(const noir::Error& err) {
  return std::make_exception_ptr(std::error_code(err.value(), err.category()));
}

} // namespace policy::detail

BOOST_OUTCOME_V2_NAMESPACE_END
