// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <commands/commands.h>
#include <iostream>

namespace commands {

CLI::App_p version = []() {
  auto cmd = std::make_shared<CLI::App>("Show version info", "version");
  cmd->parse_complete_callback([]() {
    std::cout << "v0.0.1" << std::endl;
    throw CLI::Success();
  });
  return cmd;
}();

} // namespace commands
