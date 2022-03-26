// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <appbase/application.hpp>
#include <commands/commands.h>

extern void init_app();

int main(int argc, char** argv) {
  init_app();

  auto& root_cmd = app.config();
  // clang-format off
  commands::add_command(root_cmd,
    commands::run_node,
    commands::version
  );
  // clang-format on

  return app.run(argc, argv);
}
