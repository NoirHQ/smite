// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/log/log.h>
#include <noir/thread/thread_name.h>
#include <spdlog/pattern_formatter.h>

namespace noir::log {

class thread_name_flag : public spdlog::custom_flag_formatter {
public:
  void format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest) override {
    auto& thread_name = noir::thread::thread_name();
    dest.append(thread_name.data(), thread_name.data() + thread_name.size());
  }

  std::unique_ptr<custom_flag_formatter> clone() const override {
    return spdlog::details::make_unique<thread_name_flag>();
  }
};
void set_level(const std::string& level) {
  spdlog::set_level(spdlog::level::from_str(level));
}

void setup(spdlog::logger* logger) {
  static const char* pattern = "%^%l%$\t%Y-%m-%dT%T.%e\t%t\t%s:%#\t%!\t] %v";
  auto formatter = std::make_unique<spdlog::pattern_formatter>(pattern, spdlog::pattern_time_type::utc);
  formatter->add_flag<thread_name_flag>('t');
  if (logger) {
    logger->set_formatter(std::move(formatter));
  } else {
    spdlog::set_formatter(std::move(formatter));
  }
}

} // namespace noir::log
