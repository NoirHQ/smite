// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/time.h>
#include <noir/config/defaults.h>

namespace noir::config {

struct ConsensusConfig {
  std::filesystem::path root_dir;
  std::filesystem::path wal_path;
  std::filesystem::path wal_file; ///< overrides WalPath if set

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

  ConsensusConfig() {
    wal_path = default_data_dir / "cs.wal";
    timeout_propose = std::chrono::milliseconds{3000};
    timeout_propose_delta = std::chrono::milliseconds{500};
    timeout_prevote = std::chrono::milliseconds{1000};
    timeout_prevote_delta = std::chrono::milliseconds{500};
    timeout_precommit = std::chrono::milliseconds{1000};
    timeout_precommit_delta = std::chrono::milliseconds{500};
    timeout_commit = std::chrono::milliseconds{1000};
    skip_timeout_commit = false;
    create_empty_blocks = true;
    create_empty_blocks_interval = std::chrono::seconds{0};
    peer_gossip_sleep_duration = std::chrono::milliseconds{100};
    peer_query_maj_23_sleep_duration = std::chrono::milliseconds{2000};
    double_sign_check_height = 0;
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
  tstamp commit(tstamp t) {
    return t + duration_cast<std::chrono::microseconds>(timeout_commit).count();
  }
};

} // namespace noir::config
