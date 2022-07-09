// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/block_executor.h>
#include <noir/consensus/consensus_reactor.h>
#include <noir/consensus/indexer/indexer_service.h>
#include <noir/consensus/indexer/sink/sink.h>
#include <noir/consensus/privval/file.h>
#include <noir/consensus/state.h>
#include <noir/consensus/types/genesis.h>
#include <noir/consensus/types/node_key.h>
#include <noir/consensus/types/priv_validator.h>

namespace noir::consensus {

namespace block_sync {
  struct reactor;
}
namespace ev {
  struct reactor;
}

struct node {

  std::shared_ptr<config> config_{};
  std::shared_ptr<genesis_doc> genesis_doc_{};
  std::shared_ptr<priv_validator> priv_validator_{};

  std::shared_ptr<node_key> node_key_{};
  bool is_listening{};

  std::shared_ptr<db_store> store_{};
  std::shared_ptr<block_store> block_store_{};
  std::shared_ptr<events::event_bus> event_bus_{};
  std::shared_ptr<indexer::event_sink> event_sink_{};
  std::shared_ptr<indexer::indexer_service> indexer_service_{};
  bool state_sync_on{};

  std::shared_ptr<consensus_reactor> cs_reactor{};
  std::shared_ptr<block_sync::reactor> bs_reactor{};
  std::shared_ptr<ev::reactor> ev_reactor{};

  static std::unique_ptr<node> new_default_node(appbase::application& app, const std::shared_ptr<config>& new_config);

  static std::unique_ptr<node> make_node(appbase::application& app,
    const std::shared_ptr<config>& new_config,
    const std::shared_ptr<priv_validator>& new_priv_validator,
    const std::shared_ptr<node_key>& new_node_key,
    const std::shared_ptr<genesis_doc>& new_genesis_doc,
    const std::shared_ptr<noir::db::session::session<noir::db::session::rocksdb_t>>& session);

  static void log_node_startup_info(state& state_, pub_key& pub_key_, node_mode mode);

  static std::shared_ptr<block_sync::reactor> create_block_sync_reactor(appbase::application& app,
    state& state_,
    const std::shared_ptr<block_executor>& block_exec_,
    const std::shared_ptr<block_store>& new_block_store,
    bool block_sync_);

  static Result<std::tuple<std::shared_ptr<ev::reactor>, std::shared_ptr<ev::evidence_pool>>> create_evidence_reactor(
    appbase::application& app,
    const std::shared_ptr<config>& new_config,
    const std::shared_ptr<noir::db::session::session<noir::db::session::rocksdb_t>>& session,
    const std::shared_ptr<block_store>& new_block_store);

  static std::tuple<std::shared_ptr<consensus_reactor>, std::shared_ptr<consensus_state>> create_consensus_reactor(
    appbase::application& app,
    const std::shared_ptr<config>& config_,
    const std::shared_ptr<state>& state_,
    const std::shared_ptr<block_executor>& block_exec_,
    const std::shared_ptr<block_store>& block_store_,
    const std::shared_ptr<ev::evidence_pool>& ev_pool_,
    const std::shared_ptr<priv_validator>& priv_validator_,
    const std::shared_ptr<events::event_bus>& event_bus_,
    bool wait_sync);

  void on_start();

  void on_stop();

  /// \brief load state from the database, or create one using the given genDoc
  static state load_state_from_db_or_genesis(
    const std::shared_ptr<noir::consensus::db_store>& state_store, const std::shared_ptr<genesis_doc>& gen_doc);
};

} // namespace noir::consensus
