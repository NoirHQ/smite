#pragma once
#include <fc/log/logger_config.hpp>

namespace noir::log {

static const char* default_logger_name = "tmlog";
void initialize(const char* logger_name);

#define FC_DEBUG_LOG(LOGGER_NAME, FORMAT, ...) fc_dlog(fc::logger::get(LOGGER_NAME), FORMAT, __VA_ARGS__)
#define FC_INFO_LOG(LOGGER_NAME, FORMAT, ...)  fc_ilog(fc::logger::get(LOGGER_NAME), FORMAT, __VA_ARGS__)
#define FC_WARN_LOG(LOGGER_NAME, FORMAT, ...)  fc_wlog(fc::logger::get(LOGGER_NAME), FORMAT, __VA_ARGS__)
#define FC_ERROR_LOG(LOGGER_NAME, FORMAT, ...) fc_elog(fc::logger::get(LOGGER_NAME), FORMAT, __VA_ARGS__)

#ifdef dlog
#undef dlog
#define dlog(FORMAT, ...) FC_DEBUG_LOG(noir::log::default_logger_name, FORMAT, __VA_ARGS__)
#endif

#ifdef ilog
#undef ilog
#define ilog(FORMAT, ...) FC_INFO_LOG(noir::log::default_logger_name, FORMAT, __VA_ARGS__)
#endif

#ifdef wlog
#undef wlog
#define wlog(FORMAT, ...) FC_WARN_LOG(noir::log::default_logger_name, FORMAT, __VA_ARGS__)
#endif

#ifdef elog
#undef elog
#define elog(FORMAT, ...) FC_ERROR_LOG(noir::log::default_logger_name, FORMAT, __VA_ARGS__)
#endif


} // namespace noir::tendermint::log
