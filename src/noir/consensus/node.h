// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/log.h>
#include <noir/consensus/block_executor.h>
#include <noir/consensus/common_test.h>
#include <noir/consensus/consensus_reactor.h>
#include <noir/consensus/node_key.h>
#include <noir/consensus/node_setup.h>
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
    auto [gen_doc, priv_vals] = rand_genesis_doc(*new_config, 1, false, 10);

    // Load or generate node_key - todo
    auto node_key_ = node_key::gen_node_key();

    auto db_dir = appbase::app().home_dir() / "db";
    auto session =
      std::make_shared<noir::db::session::session<noir::db::session::rocksdb_t>>(make_session(false, db_dir));

    return make_node(new_config, priv_vals[0], node_key_, std::make_shared<genesis_doc>(gen_doc), session);
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

  void on_start() {
    // Check genesis time and sleep until time is ready // todo

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
