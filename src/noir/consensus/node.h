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

  static std::unique_ptr<node> new_default_node() {
    // Load or generate priv - todo
    priv_validator local_validator_;
    local_validator_.pub_key_.key = std::vector<char>(from_hex("000000"));

    // Load or generate node_key - todo
    auto node_key_ = node_key::gen_node_key();

    //    make_node(local_validator_, node_key_);
  }

  static std::unique_ptr<node> make_node(const std::shared_ptr<config>& new_config,
    const priv_validator& new_priv_validator, const node_key& new_node_key,
    const std::shared_ptr<genesis_doc>& new_genesis_doc) {

    auto session = std::make_shared<noir::db::session::session<noir::db::session::rocksdb_t>>(make_session(false));
    auto dbs = std::make_shared<noir::consensus::db_store>(session);
    auto proxyApp = std::make_shared<app_connection>();
    auto mempool = std::make_shared<tx_pool::tx_pool>();
    auto bls = std::make_shared<noir::consensus::block_store>(session);
    auto block_exec = block_executor::new_block_executor(dbs, proxyApp, mempool, bls);

    state state_ = load_state_from_db_or_genesis(); // todo - properly load state

    auto cs = consensus_state::new_state(new_config->consensus, state_, block_exec, bls);

    auto new_cs_reactor = consensus_reactor::new_consensus_reactor(cs, false);

    // node_->local_config = config::get_default();
    // Check config.Mode == cfg.ModeValidator

    auto node_ = std::make_unique<node>();
    node_->config_ = new_config;
    node_->genesis_doc_ = new_genesis_doc;
    node_->priv_validator_ = new_priv_validator;
    node_->node_key_ = new_node_key;
    node_->store_ = dbs;
    node_->block_store_ = bls;
    node_->cs_reactor = new_cs_reactor;
    return node_;
  }

  void log_node_startup_info(bytes pub_key, std::string mode) {
    ilog("Version info tmVersion=0.0.0, block=, p2p=, mode=${mode}", ("mode", mode));

    if (mode == "ModeFull")
      ilog("This node is a fullnode");
    if (mode == "ModeValidator") {
      auto addr = pub_key; // todo - convert pub_key to addr
      ilog("This node is a validator addr=${addr} pubKey=${pubKey}", ("addr", addr)("pubKey", pub_key));
    }
  }

  void on_start() {}

  void on_stop() {}

  /**
   * load state from the database, or create one using the given genDoc.
   * On success this returns the genesis doc loaded through the given provider.
   */
  static state load_state_from_db_or_genesis(/* stateStore, genDoc*/) {
    // todo
    // 1. Attempt to load state form the database
    state state_;

    if (state_.is_empty()) {
      // 2. If it's not there, derive it from the genesis doc
      // state_ = state::make_genesis_state();
    }
    return state_;
  }
};

} // namespace noir::consensus
