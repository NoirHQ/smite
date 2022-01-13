// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/log.h>
#include <noir/consensus/consensus_reactor.h>
#include <noir/consensus/node_key.h>
#include <noir/consensus/priv_validator.h>
#include <noir/consensus/state.h>

namespace noir::consensus {

class node : boost::noncopyable {
public:
  node();

  static std::unique_ptr<node> new_default_node();
  static std::unique_ptr<node> make_node(priv_validator local_validator_, node_key node_key_);

  void log_node_startup_info(bytes pub_key, std::string mode);

  void on_start();

  void on_stop();

  static state load_state_from_db_or_genesis(/* stateStore, genDoc*/);

private:

  // config
//  config        *cfg.Config
  config local_config;

//  genesisDoc    *types.GenesisDoc   // initial validator set

//  privValidator types.PrivValidator // local node's validator key
  priv_validator local_validator;

  // network
//  transport   *p2p.MConnTransport
//  sw          *p2p.Switch // p2p connections
//  peerManager *p2p.PeerManager
//  router      *p2p.Router
//  addrBook    pex.AddrBook // known peers
//  nodeInfo    types.NodeInfo

//  nodeKey     types.NodeKey // our node privkey
  node_key local_node_key;

//  isListening bool

  // services
//  eventBus         *types.EventBus // pub/sub for services
//  stateStore       sm.Store
//  blockStore       *store.BlockStore // store the blockchain to disk
//  bcReactor        service.Service   // for block-syncing
//  mempoolReactor   service.Service   // for gossipping transactions
//  mempool          mempool.Mempool
//  stateSync        bool               // whether the node should state sync on startup
//  stateSyncReactor *statesync.Reactor // for hosting and restoring state sync snapshots

//  consensusReactor *cs.Reactor        // for participating in the consensus
  std::unique_ptr<consensus_reactor> cs_reactor;

  //  pexReactor       service.Service    // for exchanging peer addresses
//  evidenceReactor  service.Service
//  rpcListeners     []net.Listener // rpc servers
//  indexerService   service.Service
//  rpcEnv           *rpccore.Environment
};

std::unique_ptr<node> node::new_default_node() {
  // Load or generate priv - todo
  priv_validator local_validator_;
  local_validator_.pub_key = std::vector<char>(from_hex("000000"));

  // Load or generate node_key - todo
  auto node_key_ = node_key::gen_node_key();

  make_node(local_validator_, node_key_);
}

std::unique_ptr<node> node::make_node(priv_validator local_validator_, node_key node_key_) {
  auto node_ = std::make_unique<node>();

  node_->local_validator = local_validator_;
  node_->local_node_key = node_key_;

  node_->local_config = config::default_config();
  // Check config.Mode == cfg.ModeValidator

  // Determine whether we should attempt state sync.

  // Reload the state. It will have the Version.Consensus.App set by the
  // Handshake, and may have other modifications as well (ie. depending on
  // what happened during block replay).
  state state_ = load_state_from_db_or_genesis(); // todo - properly load state

  node_->log_node_startup_info(node_->local_node_key.get_pub_key(), "ModeValidator");

  // Create mempool // todo - here? or somewhere?

  // Create consensus_reactor
  node_->cs_reactor = consensus_reactor::new_consensus_reactor(node_->local_config, state_);

  return node_;
}

void node::log_node_startup_info(bytes pub_key, std::string mode) {
  ilog("Version info tmVersion=0.0.0, block=, p2p=, mode=${mode}", ("mode", mode));

  if (mode == "ModeFull")
    ilog("This node is a fullnode");
  if (mode == "ModeValidator") {
    auto addr = pub_key; // todo - convert pub_key to addr
    ilog("This node is a validator addr=${addr} pubKey=${pubKey}", ("addr", addr)("pubKey", pub_key));
  }
}

/**
 * load state from the database, or create one using the given genDoc.
 * On success this returns the genesis doc loaded through the given provider.
 */
state node::load_state_from_db_or_genesis(/* stateStore, genDoc*/) {
  // todo
  // 1. Attempt to load state form the database
  state state_;

  if (state_.is_empty()) {
    // 2. If it's not there, derive it from the genesis doc
    state_ = state::make_genesis_state();
  }
  return state_;
}

} // namespace noir::consensus
