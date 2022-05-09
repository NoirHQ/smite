#include <tendermint/log/log.h>
#include <tendermint/log/setup.h>

#include <spdlog/sinks/ansicolor_sink.h>

namespace tendermint::log {

auto make_unique(const std::string& name) -> std::unique_ptr<Logger> {
  auto color_sink = std::make_shared<spdlog::sinks::ansicolor_stdout_sink_mt>();
  auto logger = std::make_unique<Logger>(name, std::move(color_sink));
  setup(logger.get());
  return logger;
}

} // namespace tendermint::log
