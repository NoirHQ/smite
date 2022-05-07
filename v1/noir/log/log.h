// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <spdlog/spdlog.h>

namespace noir::log {

using Logger = spdlog::logger;

inline auto default_logger() {
  return spdlog::default_logger();
}

} // namespace noir::log

#if defined(ilog)
#undef ilog
#endif

#if defined(dlog)
#undef dlog
#endif

#if defined(wlog)
#undef wlog
#endif

#if defined(elog)
#undef elog
#endif

#define ilog(FORMAT, ...) SPDLOG_INFO(FORMAT __VA_OPT__(, ) __VA_ARGS__)
#define dlog(FORMAT, ...) SPDLOG_DEBUG(FORMAT __VA_OPT__(, ) __VA_ARGS__)
#define wlog(FORMAT, ...) SPDLOG_WARN(FORMAT __VA_OPT__(, ) __VA_ARGS__)
#define elog(FORMAT, ...) SPDLOG_ERROR(FORMAT __VA_OPT__(, ) __VA_ARGS__)

#define noir_ilog(LOGGER, FORMAT, ...) SPDLOG_LOGGER_INFO(LOGGER, FORMAT __VA_OPT__(, ) __VA_ARGS__)
#define noir_dlog(LOGGER, FORMAT, ...) SPDLOG_LOGGER_DEBUG(LOGGER, FORMAT __VA_OPT__(, ) __VA_ARGS__)
#define noir_wlog(LOGGER, FORMAT, ...) SPDLOG_LOGGER_WARN(LOGGER, FORMAT __VA_OPT__(, ) __VA_ARGS__)
#define noir_elog(LOGGER, FORMAT, ...) SPDLOG_LOGGER_ERROR(LOGGER, FORMAT __VA_OPT__(, ) __VA_ARGS__)
