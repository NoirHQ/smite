// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
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

  static base_config default_consensus_config() {
    return base_config{};
  }
};

struct consensus_config {
//  std::string root_dir;
  std::string wal_path;
//  std::string wal_file;

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

  static consensus_config default_consensus_config() {
    return consensus_config{"cs.wal",
      std::chrono::milliseconds{3000},
      std::chrono::milliseconds{500},
      std::chrono::milliseconds{1000},
      std::chrono::milliseconds{500},
      std::chrono::milliseconds{1000},
      std::chrono::milliseconds{500},
      std::chrono::milliseconds{1000},
      false,
      true,
      std::chrono::seconds{0},
      std::chrono::milliseconds{100},
      std::chrono::milliseconds{2000},
      0
    };
  }
};

struct config {
  base_config base;
  consensus_config consensus;

  static config default_config() {
    return {
      base_config::default_consensus_config(),
      consensus_config::default_consensus_config()
    };
  }
};

} // namespace noir::consensus
