// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>
#include <noir/common/backtrace.h>
#include <noir/common/log.h>
#include <eo/os.h>
#include <csignal>

namespace cmd = noir::commands;

using namespace noir;
using namespace eo;

int main(int argc, char** argv) {
  signal(SIGSEGV, noir::print_backtrace); // install our handler

  auto& root_cmd = *cmd::root_cmd;
  root_cmd.add_subcommand(cmd::init_files_cmd);
  root_cmd.add_subcommand(cmd::reset_all_cmd);
  root_cmd.add_subcommand(cmd::version_cmd);

  cmd::prepare_base_cmd(root_cmd, os::expand_env("$HOME" / config::default_tendermint_dir));
  cmd::parse_config(argc, argv);

  CLI11_PARSE(root_cmd, argc, argv)

  return 0;
}
