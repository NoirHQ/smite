// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <appbase/application.hpp>

extern appbase::application app;

namespace commands {

void add_command(CLI::App& root, CLI::App_p cmd);

template<typename T, typename... Ts>
void add_command(CLI::App& root, T cmd, Ts... cmds) {
  add_command(root, cmd);
  add_command(root, cmds...);
}

extern CLI::App_p run_node;
extern CLI::App_p version;

} // namespace commands
