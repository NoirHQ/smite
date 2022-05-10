#pragma once
#include <noir/log/log.h>
#include <tendermint/log/detail/field.h>

namespace tendermint::log {

class Logger : public noir::log::Logger {
public:
  using noir::log::Logger::Logger;

  auto message_with_fields(std::string message, auto&&... keyvals) -> std::string {
    auto fs = fields;
    return detail::message_with_fields(message, fs, keyvals...);
  }

  void with(auto&&... keyvals) {
    detail::build_fields_map(fields, keyvals...);
  }

private:
  detail::Fields fields{};
};

template<typename LOGGER, typename T, typename... Ts>
auto message_with_fields(LOGGER* logger, T&& message, Ts&&... keyvals) -> std::string {
  if constexpr (std::is_base_of_v<Logger, LOGGER>) {
    return logger->message_with_fields(std::forward<T>(message), std::forward<Ts>(keyvals)...);
  } else {
    detail::Fields fs{};
    return detail::message_with_fields(message, fs, keyvals...);
  }
}

auto make_unique(const std::string& name) -> std::unique_ptr<Logger>;

} // namespace tendermint::log

#define tm_ilog(LOGGER, MESSAGE, ...) SPDLOG_LOGGER_INFO(LOGGER, tendermint::log::message_with_fields(LOGGER, MESSAGE __VA_OPT__(, ) __VA_ARGS__))
#define tm_dlog(LOGGER, MESSAGE, ...) SPDLOG_LOGGER_DEBUG(LOGGER, tendermint::log::message_with_fields(LOGGER, MESSAGE __VA_OPT__(, ) __VA_ARGS__))
#define tm_wlog(LOGGER, MESSAGE, ...) SPDLOG_LOGGER_WARN(LOGGER, tendermint::log::message_with_fields(LOGGER, MESSAGE __VA_OPT__(, ) __VA_ARGS__))
#define tm_elog(LOGGER, MESSAGE, ...) SPDLOG_LOGGER_ERROR(LOGGER, tendermint::log::message_with_fields(LOGGER, MESSAGE __VA_OPT__(, ) __VA_ARGS__))

#ifdef ilog
# undef ilog
# define ilog(MESSAGE, ...) tm_ilog(noir::log::default_logger_raw(), MESSAGE, __VA_ARGS__)
#endif

#ifdef dlog
# undef dlog
# define dlog(MESSAGE, ...) tm_dlog(noir::log::default_logger_raw(), MESSAGE, __VA_ARGS__)
#endif

#ifdef wlog
# undef wlog
# define wlog(MESSAGE, ...) tm_wlog(noir::log::default_logger_raw(), MESSAGE, __VA_ARGS__)
#endif

#ifdef elog
# undef elog
# define elog(MESSAGE, ...) tm_elog(noir::log::default_logger_raw(), MESSAGE, __VA_ARGS__)
#endif
