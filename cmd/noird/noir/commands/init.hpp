#pragma once
#include <appbase/application.hpp>

namespace noir::commands {

CLI::App* init(CLI::App& root) {
  auto cmd = root.add_subcommand("init", "Initialize a NOIR node")->final_callback([]() {
    auto& app = appbase::app();
    auto config_file = app.config_file();
    noir::tendermint::config::load(config_file.c_str());

    auto cmd = app.cli().get_subcommand("init");
    auto mode = cmd->get_option("mode")->as<std::string>();
    auto key_type = cmd->get_option("--key")->as<std::string>();

    noir::tendermint::config::set("mode", mode.c_str());
    noir::tendermint::config::set("key_type", key_type.c_str());
    noir::tendermint::config::save();
  });
  cmd->add_option("mode", "Initialization mode")->required()->
    check(CLI::IsMember({"full", "validator", "seed"}));
  cmd->add_option("--key", "Key type to generate privval file with.")->
    check(CLI::IsMember({"ed25519", "secp256k1"}))->default_str("ed25519");
  return cmd;
}

} // namespace noir::conmmands
