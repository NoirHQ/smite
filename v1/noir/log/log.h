// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <spdlog/spdlog.h>

namespace noir::log {

using Logger = spdlog::logger;

const auto default_logger = spdlog::default_logger;
const auto default_logger_raw = spdlog::default_logger_raw;
const auto set_level = spdlog::set_level;

} // namespace noir::log

#define noir_ilog(LOGGER, FORMAT, ...) SPDLOG_LOGGER_INFO(LOGGER, FORMAT __VA_OPT__(, ) __VA_ARGS__)
#define noir_dlog(LOGGER, FORMAT, ...) SPDLOG_LOGGER_DEBUG(LOGGER, FORMAT __VA_OPT__(, ) __VA_ARGS__)
#define noir_wlog(LOGGER, FORMAT, ...) SPDLOG_LOGGER_WARN(LOGGER, FORMAT __VA_OPT__(, ) __VA_ARGS__)
#define noir_elog(LOGGER, FORMAT, ...) SPDLOG_LOGGER_ERROR(LOGGER, FORMAT __VA_OPT__(, ) __VA_ARGS__)

#define ilog(FORMAT, ...) noir_ilog(noir::default_logger_raw(), FORMAT __VA_OPT__(, ) __VA_ARGS__)
#define dlog(FORMAT, ...) noir_dlog(noir::default_logger_raw(), FORMAT __VA_OPT__(, ) __VA_ARGS__)
#define wlog(FORMAT, ...) noir_wlog(noir::default_logger_raw(), FORMAT __VA_OPT__(, ) __VA_ARGS__)
#define elog(FORMAT, ...) noir_elog(noir::default_logger_raw(), FORMAT __VA_OPT__(, ) __VA_ARGS__)
