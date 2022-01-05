// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <appbase/application.hpp>

namespace noir::commands {

CLI::App* debug(CLI::App& root) {
  auto cmd = root.add_subcommand("debug", "Debugging mode")->final_callback([]() {
  });
  cmd->group("");
  return cmd;
}

} // namespace noir::commands
