// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/commands/commands.h>
#include <noir/core/core.h>
/*
#include <noir/common/log.h>
#include <noir/consensus/config.h>
#include <noir/consensus/privval/file.h>
#include <noir/consensus/types/genesis.h>
#include <noir/consensus/types/node_key.h>
#include <noir/consensus/types/priv_validator.h>
*/

//using namespace noir::consensus;

namespace noir::commands {

std::string key_type;

auto init_files_with_config(const config::Config& config) -> Result<void> {
  return success();
}

auto init_files_cmd = []() {
  auto cmd = std::make_shared<CLI::App>("Initializes a Smite node", "init");
  cmd->final_callback([&self{*cmd}]() {
    auto& cfg = *config();
    cfg.mode = self.get_option("mode")->as<std::string>();
    init_files_with_config(cfg);

    auto opt = self.get_parent()->get_option_no_throw("--home");
    std::cout << opt->as<std::filesystem::path>() << std::endl;
  });
  cmd->fallthrough();
  cmd->add_option("mode", "Initialization mode")->required()->check(CLI::IsMember({"full", "validator", "seed"}));
  cmd->add_option("--key", key_type, "Key type to generate privval file with.")
    ->check(CLI::IsMember({"ed25519"}))
    ->default_str("ed25519")
    ->run_callback_for_default();
  return cmd;
}();

/*
CLI::App* init(CLI::App& root) {
  auto cmd = root.add_subcommand("init", "Initialize a NOIR node")->final_callback([&root]() {
    auto config_ = config::get_default();
    auto home_dir = app.home_dir();
    auto config_dir = home_dir / default_config_dir;
    ilog("home_dir = ${dir}", ("dir", home_dir.string()));
    ilog("config_dir = ${config_dir}", ("config_dir", config_dir.string()));

    auto init_options = root.get_subcommand("init");
    mode = init_options->get_option("mode")->as<std::string>();
    if (mode == "seed") {
      throw CLI::ValidationError("mode: seed node is not supported for now");
    }
    key_type = init_options->get_option("--key")->as<std::string>();
    if (key_type != "ed25519") {
      throw CLI::ValidationError("key: only ed25519 is supported");
    }

    std::vector<genesis_validator> validators;
    if (mode == "validator") {
      // generate priv_validator_key.json
      std::vector<std::shared_ptr<priv_validator>> priv_validators;
      auto priv_val = privval::file_pv::load_or_gen_file_pv(
        home_dir / config_.priv_validator.key, home_dir / config_.priv_validator.state);
      if (!priv_val) {
        throw CLI::FileError(priv_val.error().message());
      }
      auto vote_power = 10;
      auto val = validator{priv_val.value()->get_address(), priv_val.value()->get_pub_key(), vote_power, 0};
      validators.push_back(genesis_validator{val.address, val.pub_key_, val.voting_power});
      priv_validators.push_back(std::move(priv_val.value()));
    }

    // generate genesis.json
    std::shared_ptr<genesis_doc> gen_doc{};
    if (auto ok = genesis_doc::genesis_doc_from_file(config_dir / "genesis.json"); !ok) {
      gen_doc = std::make_shared<genesis_doc>(genesis_doc{get_time(), config_.base.chain_id, 0, {}, validators});
      gen_doc->save(config_dir / "genesis.json");
    } else {
      gen_doc = ok.value();
    }

    // generate node_key.json
    node_key::load_or_gen_node_key(config_dir / config_.base.node_key);
    ilog("done");
  });
  cmd->add_option("mode", mode, "Initialization mode")->required()->check(CLI::IsMember({"full", "validator", "seed"}));
  cmd->add_option("--key", key_type, "Key type to generate privval file with.")
    ->check(CLI::IsMember({"ed25519"}))
    ->default_str("ed25519")
    ->run_callback_for_default();
  return cmd;
}
*/

} // namespace noir::commands
