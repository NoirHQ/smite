#include <noir/common/log.h>

#include <fc/log/logger_config.hpp>

#include <noir/tendermint/tendermint.h>

namespace noir::log {

  tmlog_appender::tmlog_appender(const fc::variant &args) {}

  tmlog_appender::tmlog_appender() {}

  tmlog_appender::~tmlog_appender() {}

  void tmlog_appender::initialize(boost::asio::io_service &io_service) {}

  void tmlog_appender::log(const fc::log_message &m) {
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

  void initialize( const char* logger_name ) {
    fc::log_config::register_appender<tmlog_appender>(logger_name);
    const fc::logging_config logging_config = [logger_name](){
      fc::logging_config logging_config;
      fc::variants c;
      c.push_back(fc::mutable_variant_object("level", "debug")("color", "green"));
      c.push_back(fc::mutable_variant_object("level", "warn")("color", "yellow"));
      c.push_back(fc::mutable_variant_object("level", "error")("color", "red"));

      logging_config.appenders.emplace_back("default", logger_name, fc::mutable_variant_object()
              ("stream", "default")
              ("level_colors", c)
      );

      fc::logger_config dlc;
      dlc.name = logger_name;
      dlc.level = fc::log_level::all;
      dlc.appenders.emplace_back("default");
      logging_config.loggers.push_back(dlc);

      return logging_config;
    }();
    fc::log_config::configure_logging(logging_config);
  }
} // namespace noir::log
