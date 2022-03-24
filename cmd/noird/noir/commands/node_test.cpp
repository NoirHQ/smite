// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>
#include <noir/consensus/abci.h>
#include <noir/p2p/p2p.h>

namespace noir::commands {

CLI::App* node_test(CLI::App& root) {
  app.register_plugin<consensus::abci>();
  app.register_plugin<p2p::p2p>();
  auto cmd = root.add_subcommand("node_test", "Sample to test running a node")->final_callback([]() {
    fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::info);
    app.initialize<consensus::abci, p2p::p2p>();
    app.startup();
    app.exec();
  });
  cmd->group("");
  return cmd;
}

} // namespace noir::commands
