// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/log/log.h>

namespace noir::log {

void set_level(const std::string& level) {
  spdlog::set_level(spdlog::level::from_str(level));
}

void setup(spdlog::logger* logger) {
  static const char* pattern = "%^%l%$\t%Y-%m-%dT%T.%e\t%t\t%s:%#\t%!\t] %v";
  if (logger) {
    logger->set_pattern(pattern, spdlog::pattern_time_type::utc);
  } else {
    spdlog::set_pattern(pattern, spdlog::pattern_time_type::utc);
  }
}

} // namespace noir::log
