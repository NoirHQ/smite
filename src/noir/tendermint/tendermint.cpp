#include <noir/tendermint/tendermint.h>

#include <noir/common/log.h>

extern void abci_init();

namespace noir::tendermint {

void tendermint::set_program_options(CLI::App& cli, CLI::App& config) {
  abci_init();
}

void tendermint::plugin_initialize(const CLI::App& cli, const CLI::App& config) {
  noir::log::initialize(noir::log::DEFAULT_LOGGER_NAME);
  log::info("tendermint init");
}

void tendermint::plugin_startup() {
  log::info("tendermint start");
  node::start();
}

void tendermint::plugin_shutdown() {
  log::info("tendermint stop");
  node::stop();
}

} // namespace noir::tendermint
