// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>
#include <iostream>

namespace noir::commands {

CLI::App* version(CLI::App& root) {
  return root.add_subcommand("version", "Show version info")->parse_complete_callback([]() {
    std::cout << "v0.0.1" << std::endl;
    throw CLI::Success();
  });
}

} // namespace noir::commands
