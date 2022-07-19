// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>

namespace noir::commands {

auto config() -> std::shared_ptr<config::Config> {
  static auto c = std::make_shared<config::Config>();
  return c;
}

auto root_cmd = []() {
  auto cmd = std::make_shared<CLI::App>(
    "BFT state machine replication for applications in any programming languages",
    "smite");
  cmd->preparse_callback([](std::size_t) {
  });
  cmd->require_subcommand();
  cmd->failure_message(CLI::FailureMessage::help);
  cmd->set_help_all_flag("--help-all")->group("");
  config()->bind(*cmd);
  return cmd;
}();

} // namespace noir::commands
