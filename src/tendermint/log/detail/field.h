#pragma once
#include <fmt/color.h>
#include <fmt/format.h>
#include <map>

namespace tendermint::log {

namespace detail {
  using Fields = std::map<std::string, std::string>;

  inline void add_field(Fields& fs) {}
  inline void add_field(Fields& fs, std::string key, auto&& value, auto&&... keyvals) {
    fs[key] = fmt::to_string(value);
    add_field(fs, keyvals...);
  }
  void build_fields_map(Fields& fs, auto&&... keyvals) {
    add_field(fs, keyvals...);
  }

  std::string message_with_fields(std::string message, Fields& fs, auto&&... keyvals) {
    static_assert(sizeof...(keyvals) % 2 == 0);
    detail::build_fields_map(fs, keyvals...);
    std::for_each(fs.begin(), fs.end(), [&message](auto& f) {
      message += ' ';
      message += fmt::format(fmt::fg(fmt::terminal_color::cyan), f.first + '=');
      message += f.second;
    });
    return message;
  }
} // namespace detail

} // namespace tendermint::log
