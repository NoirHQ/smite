// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/tendermint/tendermint.h>
#include <appbase/application.hpp>

using namespace noir::tendermint;

static std::string mode;
static std::string key_type;

namespace noir::commands {

CLI::App* init(CLI::App& root) {
  auto cmd = root.add_subcommand("init", "Initialize a NOIR node")->final_callback([]() {
    auto home_dir = appbase::app().home_dir();
    config::set("home", home_dir.c_str());
    config::load();

    config::set("mode", mode.c_str());
    config::set("key", key_type.c_str());
    config::save();
  });
  cmd->add_option("mode", mode, "Initialization mode")->required()->check(CLI::IsMember({"full", "validator", "seed"}));
  cmd->add_option("--key", key_type, "Key type to generate privval file with.")
    ->check(CLI::IsMember({"ed25519", "secp256k1"}))
    ->default_str("ed25519")
    ->run_callback_for_default();
  return cmd;
}

} // namespace noir::commands
