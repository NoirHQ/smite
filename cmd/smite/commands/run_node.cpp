// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <commands/commands.h>
#include <tendermint/node/plugin.h>

namespace commands {

CLI::App_p run_node = []() {
  auto cmd = std::make_shared<CLI::App>("Run the tendermint node", "start");
  cmd->alias("node")->alias("run");
  cmd->parse_complete_callback([&]() {
    app.initialize<tendermint::NodePlugin>();
    app.startup();
    app.exec();
  });
  return cmd;
}();

} // namespace commands
