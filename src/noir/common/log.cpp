// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/log.h>
#include <fc/log/log_message.hpp>
#include <fc/log/logger_config.hpp>

using namespace fc;

namespace noir::log {

const char* default_logger_name = "console";

void initialize(const char* logger_name) {
  log_config::configure_logging(logging_config::default_config());
}

} // namespace noir::log
