#include <tendermint/log/setup.h>

#include <fmt/color.h>
#include <spdlog/details/fmt_helper.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/ansicolor_sink.h>

namespace tendermint::log {

namespace fmt_helper = spdlog::details::fmt_helper;

struct level_formatter : public spdlog::custom_flag_formatter {
  void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
    static std::string_view levels[] = {"TRC", "DBG", "INF", "WRN", "ERR", "FTL", "OFF"};
    fmt_helper::append_string_view(levels[msg.level], dest);
  }

  std::unique_ptr<custom_flag_formatter> clone() const override {
    return std::make_unique<level_formatter>();
  }
};

struct r_formatter : public spdlog::custom_flag_formatter {
  int hour(int h) {
    return (h %= 12) ? h : 12;
  }

  void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(msg.time.time_since_epoch());
    if (secs != last_log_secs) {
      auto curr = spdlog::log_clock::to_time_t(msg.time);
      cached_tm = *std::localtime(&curr);
      last_log_secs = secs;
    }
    auto formatted = fmt::format(fmt::fg(fmt::terminal_color::bright_black), fmt::format(
      "{:d}:{:02d}{:s}", hour(cached_tm.tm_hour), cached_tm.tm_min, cached_tm.tm_hour / 12 ? "PM" : "AM"));
    fmt_helper::append_string_view(formatted, dest);
  }

  std::unique_ptr<custom_flag_formatter> clone() const override {
    return std::make_unique<r_formatter>();
  }

  std::tm cached_tm;
  std::chrono::seconds last_log_secs;
};

void setup(Logger* logger) {
  static const char* pattern = "%r %^%l%$ %v";
  auto formatter = std::make_unique<spdlog::pattern_formatter>();
  formatter->add_flag<level_formatter>('l');
  formatter->add_flag<r_formatter>('r');
  formatter->set_pattern(pattern);
  if (logger) {
    logger->set_formatter(std::move(formatter));
  } else {
    spdlog::set_formatter(std::move(formatter));
  }
}

} // namespace tendermint::log
