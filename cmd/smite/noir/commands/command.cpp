// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>

namespace noir::commands {

CLI::App* add_command(CLI::App& root, add_command_callback cb) {
  return cb(root);
}

} // namespace noir::commands
