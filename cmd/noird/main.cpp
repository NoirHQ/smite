// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>
#include <noir/common/log.h>
#include <noir/tendermint/tendermint.h>
#include <appbase/application.hpp>

using namespace noir;

int main(int argc, char** argv) {
  auto& app = appbase::app();

  // set default configuration
  app.cli().require_subcommand();
  app.cli().failure_message(CLI::FailureMessage::help);
  app.cli().name("noird");
  app.cli().description("NOIR Protocol App");
  std::filesystem::path home_dir;
  if (auto arg = std::getenv("HOME")) {
    home_dir = arg;
  }
  app.set_home_dir(home_dir / ".noir");
  app.set_config_file("app.toml");

  noir::log::initialize("tmlog");

  // add subcommands
  commands::add_command(app.cli(), &commands::debug);
  commands::add_command(app.cli(), &commands::init);
  commands::add_command(app.cli(), &commands::start);
  commands::add_command(app.cli(), &commands::version);

  // register plugins
  app.register_plugin<tendermint::tendermint>();

  return app.run(argc, argv);
}
