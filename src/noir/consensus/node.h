// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/log.h>
#include <noir/consensus/block_executor.h>
#include <noir/consensus/consensus_reactor.h>
#include <noir/consensus/node_key.h>
#include <noir/consensus/priv_validator.h>
#include <noir/consensus/state.h>

namespace noir::consensus {

struct node {

  std::shared_ptr<config> config_{};
  std::shared_ptr<genesis_doc> genesis_doc_{};
  priv_validator priv_validator_{};

  node_key node_key_{};
  bool is_listening{};

  std::shared_ptr<db_store> store_{};
  std::shared_ptr<block_store> block_store_{};
  bool state_sync_on{};

  std::shared_ptr<consensus_reactor> cs_reactor{};

  static std::unique_ptr<node> new_default_node(const std::shared_ptr<config>& new_config) {
    // Load or generate priv - todo
    std::vector<genesis_validator> validators;
    std::vector<priv_validator> priv_validators;
    auto priv_val = priv_validator{};
    priv_val.type = MockSignerClient;
    fc::crypto::private_key new_key("5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"); // TODO: read from a file
    priv_val.priv_key_.key = fc::from_base58(new_key.to_string());
    priv_val.pub_key_ = priv_val.priv_key_.get_pub_key();
    auto vote_power = 10;
    auto val = validator{priv_val.pub_key_.address(), priv_val.pub_key_, vote_power, 0};
    validators.push_back(genesis_validator{val.address, val.pub_key_, val.voting_power});
    priv_validators.push_back(priv_val);
    auto gen_doc = genesis_doc{get_time(), new_config->base.chain_id, 1, {}, validators};

    // Load or generate node_key - todo
    auto node_key_ = node_key::gen_node_key();

    auto db_dir = appbase::app().home_dir() / "db";
    auto session =
      std::make_shared<noir::db::session::session<noir::db::session::rocksdb_t>>(make_session(false, db_dir));

    return make_node(new_config, priv_validators[0], node_key_, std::make_shared<genesis_doc>(gen_doc), session);
  }

  static std::unique_ptr<node> make_node(const std::shared_ptr<config>& new_config,
    const priv_validator& new_priv_validator, const node_key& new_node_key,
    const std::shared_ptr<genesis_doc>& new_genesis_doc,
    const std::shared_ptr<noir::db::session::session<noir::db::session::rocksdb_t>>& session) {

    auto dbs = std::make_shared<noir::consensus::db_store>(session);
    auto proxyApp = std::make_shared<app_connection>();
    auto bls = std::make_shared<noir::consensus::block_store>(session);
    auto block_exec = block_executor::new_block_executor(dbs, proxyApp, bls);

    state state_ = load_state_from_db_or_genesis(dbs, new_genesis_doc);

    // Setup pub_key_
    pub_key pub_key_;
    if (new_config->base.mode == Validator) {
      pub_key_ = new_priv_validator.pub_key_;
      // todo - check error
    }

    // Setup state sync
    bool new_state_sync_on = false; // todo

    // Setup block sync
    bool block_sync = false; // todo

    log_node_startup_info(state_, pub_key_, new_config->base.mode);

    auto [new_cs_reactor, new_cs_state] = create_consensus_reactor(
      new_config, std::make_shared<state>(state_), block_exec, bls, new_priv_validator, new_state_sync_on);

    auto node_ = std::make_unique<node>();
    node_->config_ = new_config;
    node_->genesis_doc_ = new_genesis_doc;
    node_->priv_validator_ = new_priv_validator;
    node_->node_key_ = new_node_key;
    node_->store_ = dbs;
    node_->block_store_ = bls;
    node_->state_sync_on = new_state_sync_on;
    node_->cs_reactor = new_cs_reactor;
    return node_;
  }

  static void log_node_startup_info(state& state_, pub_key& pub_key_, node_mode mode) {
    ilog(fmt::format("Version info: version={}, mode={}", state_.version, mode_str(mode)));
    switch (mode) {
    case Full:
      ilog("################################");
      ilog("### This node is a full_node ###");
      ilog("################################");
      break;
    case Validator:
      ilog("#####################################");
      ilog("### This node is a validator_node ###");
      ilog("#####################################");
      {
        auto addr = pub_key_.address();
        if (state_.validators.has_address(addr))
          ilog("   > node is in the active validator set");
        else
          ilog("   > node is NOT in the active validator set");
      }
      break;
    case Seed:
      ilog("################################");
      ilog("### This node is a seed_node ###");
      ilog("################################");
      break;
    case Unknown:
      ilog("#################################");
      ilog("### This node is unknown_mode ###");
      ilog("#################################");
      break;
    }
  }

  static std::tuple<std::shared_ptr<consensus_reactor>, std::shared_ptr<consensus_state>> create_consensus_reactor(
    const std::shared_ptr<config>& config_, const std::shared_ptr<state>& state_,
    const std::shared_ptr<block_executor>& block_exec_, const std::shared_ptr<block_store>& block_store_,
    const priv_validator& priv_validator_, bool wait_sync) {
    auto cs_state = consensus_state::new_state(config_->consensus, *state_, block_exec_, block_store_);

    if (config_->base.mode == Validator)
      cs_state->set_priv_validator(priv_validator_);

    auto cs_reactor = consensus_reactor::new_consensus_reactor(cs_state, wait_sync);

    return {cs_reactor, cs_state};
  }

  void on_start() {
    // Check genesis time and sleep until time is ready // todo

    cs_reactor->on_start(); // TODO: cs_reactor->start();
    auto height = cs_reactor->cs_state->rs.height;
    auto round = cs_reactor->cs_state->rs.round;
    cs_reactor->cs_state->enter_new_round(height, round);
  }

  void on_stop() {}

  /**
   * load state from the database, or create one using the given genDoc.
   * On success this returns the genesis doc loaded through the given provider.
   */
  static state load_state_from_db_or_genesis(
    const std::shared_ptr<noir::consensus::db_store>& state_store, const std::shared_ptr<genesis_doc>& gen_doc) {
    // 1. Attempt to load state form the database
    state state_{};
    if (state_store->load(state_)) {
      dlog("successfully loaded state from db");
    } else {
      dlog("unable to load state from db");
      // return state_; // todo - what to do here?
    }

    if (state_.is_empty()) {
      // 2. If it's not there, derive it from the genesis doc
      state_ = state::make_genesis_state(*gen_doc);
    }

    return state_;
  }
};

} // namespace noir::consensus
