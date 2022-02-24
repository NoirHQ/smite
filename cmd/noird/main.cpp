// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>
#include <noir/common/log.h>
#include <noir/rpc/jsonrpc.h>
#include <noir/rpc/rpc.h>
#include <noir/tx_pool/tx_pool.h>
#include <appbase/application.hpp>
#include <eth/rpc/rpc.h>

using namespace noir;

int main(int argc, char** argv) {
  auto& app = appbase::app();

  // set default configuration
  app.config().require_subcommand();
  app.config().failure_message(CLI::FailureMessage::help);
  app.config().name("noird");
  app.config().description("NOIR Protocol App");
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
  commands::add_command(app.config(), &commands::node_test);
  commands::add_command(app.config(), &commands::p2p_test);
  commands::add_command(app.config(), &commands::start);
  commands::add_command(app.config(), &commands::version);

  // register plugins
  app.register_plugin<noir::tx_pool::tx_pool>();
  app.register_plugin<noir::rpc::rpc>();
  app.register_plugin<noir::rpc::jsonrpc>();
  app.register_plugin<eth::rpc::rpc>();

  return app.run(argc, argv);
}
