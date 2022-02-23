// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/abci.h>
#include <appbase/application.hpp>

namespace noir::commands {

CLI::App* consensus_test(CLI::App& root) {
  auto& app = appbase::app();
  app.register_plugin<tx_pool::tx_pool>();
  app.register_plugin<consensus::abci>();
  auto cmd = root.add_subcommand("consensus_test", "Sample to test consensus")->final_callback([&app]() {
    fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);
    app.initialize<tx_pool::tx_pool>();
    app.initialize<consensus::abci>();
    app.startup();
    app.exec();
  });
  cmd->group("");
  return cmd;
}

} // namespace noir::commands
