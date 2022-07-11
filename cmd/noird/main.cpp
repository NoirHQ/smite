// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>
#include <noir/common/backtrace.h>
#include <noir/common/log.h>
#include <appbase/application.hpp>
#include <signal.h>

using namespace noir;

appbase::application app;

int main(int argc, char** argv) {
  signal(SIGSEGV, noir::print_backtrace); // install our handler

  // set default configuration
  app.config().require_subcommand();
  app.config().failure_message(CLI::FailureMessage::help);
  app.config().name("noird");
  app.config().description("NOIR Protocol App");
  app.config().set_help_all_flag("--help-all")->group("");
  std::filesystem::path home_dir;
  if (auto arg = std::getenv("HOME")) {
    home_dir = arg;
  }
  app.set_home_dir(home_dir / ".noir");
  app.set_config_file("app.toml");

  noir::log::initialize("default");

  // add subcommands
  commands::add_command(app.config(), &commands::consensus_test);
  commands::add_command(app.config(), &commands::debug);
  commands::add_command(app.config(), &commands::init);
  commands::add_command(app.config(), &commands::start);
  commands::add_command(app.config(), &commands::unsafe_reset_all);
  commands::add_command(app.config(), &commands::version);

  return app.run(argc, argv);
}
