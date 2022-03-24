// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>
#include <noir/p2p/p2p.h>
#include <fc/log/logger.hpp>

namespace noir::commands {

CLI::App* p2p_test(CLI::App& root) {
  app.register_plugin<p2p::p2p>();
  auto cmd = root.add_subcommand("p2p_test", "Sample to test p2p")->final_callback([]() {
    fc::logger::get(DEFAULT_LOGGER).set_log_level(fc::log_level::debug);
    app.initialize<p2p::p2p>();
    app.startup();
    app.exec();
  });
  cmd->group("");
  return cmd;
}

} // namespace noir::commands
