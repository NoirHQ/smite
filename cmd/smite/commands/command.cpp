// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <commands/commands.h>

namespace commands {

void add_command(CLI::App& root, CLI::App_p cmd) {
  root.add_subcommand(cmd);
}

} // namespace commands
