// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/for_each.h>
#include <noir/p2p/types.h>

namespace noir::consensus {

struct base_config {
  std::string chain_id;
  std::string root_dir;
  std::string proxy_app;
  std::string moniker;
  std::string mode;
  bool fast_sync_mode;
  std::string db_backend;
  std::string db_path;
  std::string log_level;
  std::string log_format;
  std::string genesis;
  std::string node_key;
  std::string abci;
  bool filter_peers;

  static base_config get_default() {
    base_config cfg;
    cfg.genesis = "genesis.json";
    cfg.node_key = "node_key.json";
    cfg.mode = "full";
    cfg.abci = "local";
    cfg.log_level = "info";
    cfg.db_path = "data";
    return cfg;
  }
};

struct consensus_config {
  std::string root_dir;
  std::string wal_path;
  std::string wal_file;

  std::chrono::system_clock::duration timeout_propose;
  std::chrono::system_clock::duration timeout_propose_delta;
  std::chrono::system_clock::duration timeout_prevote;
  std::chrono::system_clock::duration timeout_prevote_delta;
  std::chrono::system_clock::duration timeout_precommit;
  std::chrono::system_clock::duration timeout_precommit_delta;
  std::chrono::system_clock::duration timeout_commit;

  bool skip_timeout_commit;

  bool create_empty_blocks;
  std::chrono::system_clock::duration create_empty_blocks_interval;

  std::chrono::system_clock::duration peer_gossip_sleep_duration;
  std::chrono::system_clock::duration peer_query_maj_23_sleep_duration;

  int64_t double_sign_check_height;

  static consensus_config get_default() {
    consensus_config cfg;
    cfg.wal_file = "cs.wal";
    cfg.timeout_propose = std::chrono::milliseconds{3000};
    cfg.timeout_propose_delta = std::chrono::milliseconds{500};
    cfg.timeout_prevote = std::chrono::milliseconds{1000};
    cfg.timeout_prevote_delta = std::chrono::milliseconds{500};
    cfg.timeout_precommit = std::chrono::milliseconds{1000};
    cfg.timeout_precommit_delta = std::chrono::milliseconds{500};
    cfg.timeout_commit = std::chrono::milliseconds{1000};
    cfg.skip_timeout_commit = false;
    cfg.create_empty_blocks = true;
    cfg.create_empty_blocks_interval = std::chrono::seconds{0};
    cfg.peer_gossip_sleep_duration = std::chrono::milliseconds{100};
    cfg.peer_query_maj_23_sleep_duration = std::chrono::milliseconds{2000};
    cfg.double_sign_check_height = 0;
    return cfg;
  }

  std::chrono::system_clock::duration propose(int32_t round) const {
    return timeout_propose + (timeout_propose_delta * round);
  }

  std::chrono::system_clock::duration prevote(int32_t round) const {
    return timeout_prevote + (timeout_propose_delta * round);
  }

  std::chrono::system_clock::duration precommit(int32_t round) const {
    return timeout_precommit + (timeout_precommit_delta * round);
  }

  /**
   * returns the amount of time to wait for straggler votes after receiving 2/3+ precommits
   */
  p2p::tstamp commit(p2p::tstamp t) {
    return t + timeout_commit.count();
  }
};

struct config {
  base_config base;
  consensus_config consensus;

  static config get_default() {
    return {base_config::get_default(), consensus_config::get_default()};
  }
};

} // namespace noir::consensus

NOIR_FOR_EACH_FIELD(noir::consensus::base_config, chain_id, root_dir, proxy_app, moniker, mode, fast_sync_mode,
  db_backend, db_path, log_level, log_format, genesis, node_key, abci, filter_peers)
NOIR_FOR_EACH_FIELD(noir::consensus::consensus_config, root_dir, wal_path, wal_file, timeout_propose,
  timeout_propose_delta, timeout_prevote, timeout_prevote_delta, timeout_precommit, timeout_precommit_delta,
  timeout_commit, skip_timeout_commit, create_empty_blocks, create_empty_blocks_interval, peer_gossip_sleep_duration,
  peer_query_maj_23_sleep_duration, double_sign_check_height)
NOIR_FOR_EACH_FIELD(noir::consensus::config, base, consensus)
