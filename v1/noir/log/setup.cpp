// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/log/log.h>
#include <noir/thread/thread_name.h>
#include <spdlog/details/fmt_helper.h>
#include <spdlog/pattern_formatter.h>

namespace noir::log {

namespace fmt_helper = spdlog::details::fmt_helper;

std::string spaces(64, ' ');

class thread_name_formatter : public spdlog::custom_flag_formatter {
public:
  void format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest) override {
    auto thread_name = std::string_view{thread::thread_name()};
    fmt_helper::append_string_view(thread_name, dest);
    if (padinfo_.enabled() && thread_name.size() < padinfo_.width_) {
      fmt_helper::append_string_view(spaces.substr(0, padinfo_.width_ - thread_name.size()), dest);
    }
  }

  std::unique_ptr<custom_flag_formatter> clone() const override {
    return std::make_unique<thread_name_formatter>();
  }
};

class source_location_formatter : public spdlog::custom_flag_formatter {
public:
  void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
    if (msg.source.empty() && padinfo_.enabled()) {
      fmt_helper::append_string_view(spaces.substr(0, padinfo_.width_), dest);
      return;
    }
    auto filename = std::string_view(msg.source.filename);
    auto pos = filename.find_last_of('/');
    fmt_helper::append_string_view(filename.substr(pos + 1, std::string_view::npos), dest);
    dest.push_back(':');
    fmt_helper::append_int(msg.source.line, dest);
    if (padinfo_.enabled()) {
      auto size = filename.size() - pos + fmt_helper::count_digits(msg.source.line);
      if (size < padinfo_.width_) {
        fmt_helper::append_string_view(spaces.substr(0, padinfo_.width_ - size), dest);
      }
    }
  }

  std::unique_ptr<custom_flag_formatter> clone() const override {
    return std::make_unique<source_location_formatter>();
  }
};

void set_level(const std::string& level) {
  spdlog::set_level(spdlog::level::from_str(level));
}

void setup(Logger* logger) {
  static const char* pattern = "%^%-5l%$ %Y-%m-%dT%T.%e %-9t %-29@ %-21!] %v";
  auto formatter = std::make_unique<spdlog::pattern_formatter>(pattern, spdlog::pattern_time_type::utc);
  formatter->add_flag<source_location_formatter>('@');
  formatter->add_flag<thread_name_formatter>('t');
  if (logger) {
    logger->set_formatter(std::move(formatter));
  } else {
    spdlog::set_formatter(std::move(formatter));
  }
}

} // namespace noir::log
