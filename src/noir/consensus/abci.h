// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/node.h>
#include <appbase/application.hpp>

namespace noir::consensus {

class abci : public appbase::plugin<abci> {
public:
  APPBASE_PLUGIN_REQUIRES()

  virtual ~abci() {}

  void set_program_options(CLI::App& app_config) {
    auto abci_options = app_config.add_section("abci",
      "###############################################\n"
      "###        ABCI Configuration Options       ###\n"
      "###############################################");
    abci_options->add_option("--proxy-app", "Proxy app: one of kvstore (not supported), or noop (default \"\")")
      ->default_val("");
    abci_options->add_option("--mode", "Mode of Node: full | validator | seed (not supported)")
      ->check(CLI::IsMember({"full", "validator", "seed"}))
      ->default_val("validator");
    abci_options->add_option("--do-not-start-node", "Do not start node (debug purposes)");
    abci_options->add_option("--start-non-validator-node", "Start node in non-validator mode (debug purposes)");

    auto bs_options = app_config.add_section("blocksync",
      "######################################################\n"
      "###        Block Sync Configuration Options        ###\n"
      "######################################################");
    bs_options->add_option("--enable", "If node is behind many blocks, catch up quickly by downloading blocks")
      ->check(CLI::IsMember({"true", "false"}))
      ->default_val("true");
    bs_options
      ->add_option("--version",
        "block_sync version to use:\n"
        "  1) \"v0\" (default) - standard implementation\n"
        "  2) \"v2\" - DEPRECATED")
      ->check(CLI::IsMember({"v0"}))
      ->default_val("v0");
  }

  void plugin_initialize(const CLI::App& app_config) {
    ilog("Initialize abci");
    auto abci_options = app_config.get_subcommand("abci");
    proxy_app = abci_options->get_option("--proxy-app")->as<std::string>();
    std::string mode_str = abci_options->get_option("--mode")->as<std::string>();
    node_mode mode{Unknown};
    if (mode_str == "validator") {
      mode = Validator;
    } else if (mode_str == "full") {
      mode = Full;
    } else {
      if (mode_str == "seed")
        elog("seed mode is not supported now");
      else
        elog("unknown mode: ${mode}", ("mode", mode_str));
      return;
    }
    do_not_start_node = abci_options->get_option("--do-not-start-node")->as<bool>();

    auto bs_options = app_config.get_subcommand("blocksync");
    bool bs_enable = bs_options->get_option("--enable")->as<bool>();

    auto config_ = std::make_shared<config>(config::get_default());
    config_->base.chain_id = "test_chain";
    config_->base.proxy_app = proxy_app;
    config_->base.mode = mode;
    config_->base.fast_sync_mode = bs_enable;
    config_->base.root_dir = app.home_dir().string();
    config_->consensus.root_dir = config_->base.root_dir;
    config_->priv_validator.root_dir = config_->base.root_dir;

    if (do_not_start_node) {
      // Do not start node, but create an instance of consensus_reactor so network messages can be processed
      auto event_bus_ = std::make_shared<events::event_bus>(app);
      static std::shared_ptr<consensus_reactor> cs_reactor =
        std::make_shared<consensus_reactor>(app, std::make_shared<consensus_state>(app, event_bus_), event_bus_, false);
      return;
    }

    node_ = node::new_default_node(app, config_);

    // Setup callbacks // TODO: is this right place to setup callback?
    node_->bs_reactor->set_callback_switch_to_cs_sync(std::bind(
      &consensus_reactor::switch_to_consensus, node_->cs_reactor, std::placeholders::_1, std::placeholders::_2));

    ///< TEMPORARY
    // Do not start block_sync reactor when we are single node validator (test env)
    if (config_->base.mode == Validator) {
      // TODO: remove later; temporary workaround to prevent validator from starting bs_reactor
      node_->bs_reactor->block_sync = false;
      node_->cs_reactor->wait_sync = false;
    }
    ///< TEMPORARY ENDS
  }

  void plugin_startup() {
    if (do_not_start_node)
      return;

    node_->on_start();
  }

  void plugin_shutdown() {
    ilog("shutting down abci");
    node_->on_stop();
  }

  std::unique_ptr<node> node_;
  std::string proxy_app;
  bool do_not_start_node{false};
};

} // namespace noir::consensus
