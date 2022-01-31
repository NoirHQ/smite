// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/tendermint/tendermint.h>

extern void abci_init();

namespace noir::tendermint {

void tendermint::set_program_options(CLI::App& config) {
  abci_init();
}

void tendermint::plugin_initialize(const CLI::App& config) {
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
