// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>
#include <noir/consensus/abci.h>

namespace noir::commands {

CLI::App* consensus_test(CLI::App& root) {
  app.register_plugin<consensus::abci>();
  auto cmd = root.add_subcommand("consensus_test", "Sample to test consensus")->final_callback([]() {
    fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);
    app.initialize<consensus::abci>();
    app.startup();
    app.exec();
  });
  cmd->group("");
  return cmd;
}

} // namespace noir::commands
