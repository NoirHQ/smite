#include <noir/common/log.h>
#include <fc/log/log_message.hpp>
#include <fc/log/logger_config.hpp>

namespace noir::log {

void initialize(const char* logger_name) {
  const fc::logging_config logging_config = [logger_name]() {
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
