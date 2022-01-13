// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <fc/log/appender.hpp>
#include <fc/log/log_message.hpp>
#include <fc/log/logger_config.hpp>
#include <tendermint/tendermint.h>

namespace noir::tendermint::log {

void info(const char* msg) {
  tm_log_info(msg);
}

void debug(const char* msg) {
  tm_log_debug(msg);
}

void error(const char* msg) {
  tm_log_error(msg);
}

class appender : public fc::appender {
public:
  explicit appender(const fc::variant& args) {}

  appender() {}

  virtual ~appender() {}

  void initialize(boost::asio::io_context& io_context) override {}

  void log(const fc::log_message& m) override {
    fc::string message = format_string(m.get_format(), m.get_data());

    const fc::log_level log_level = m.get_context().get_log_level();
    switch (log_level) {
    case fc::log_level::debug:
      tendermint::log::debug(message.c_str());
      break;
    case fc::log_level::info:
      tendermint::log::info(message.c_str());
      break;
    case fc::log_level::warn:
    case fc::log_level::error:
      tendermint::log::error(message.c_str());
      break;
    }
  }
};

} // namespace noir::tendermint::log

static bool reg_appender = fc::log_config::register_appender<noir::tendermint::log::appender>("tmlog");
