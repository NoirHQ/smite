// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>
#include <eo/fmt.h>

namespace noir::commands {

auto version_cmd = []() {
  auto cmd = std::make_shared<CLI::App>("Show version info", "version");
  cmd->parse_complete_callback([]() {
    fmt::println("v0.35.0");
    throw CLI::Success();
  });
  return cmd;
}();


} // namespace noir::commands
