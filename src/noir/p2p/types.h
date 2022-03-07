// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/refl.h>
#include <noir/common/types/bytes_n.h>

#include <chrono>
#include <cinttypes>
#include <memory>
#include <mutex>
#include <vector>

namespace noir::p2p {

using tstamp = std::chrono::system_clock::duration::rep;

using block_id_type = bytes32;

/**
 * default value initializers
 */
constexpr auto def_send_buffer_size_mb = 4;
constexpr auto def_send_buffer_size = 1024 * 1024 * def_send_buffer_size_mb;
constexpr auto def_max_write_queue_size = def_send_buffer_size * 10;
constexpr auto def_max_trx_in_progress_size = 100 * 1024 * 1024; // 100 MB
constexpr auto def_max_consecutive_immediate_connection_close = 9; // back off if client keeps closing
constexpr auto def_max_clients = 25; // 0 for unlimited clients
constexpr auto def_max_nodes_per_host = 1;
constexpr auto def_conn_retry_wait = 30;
constexpr auto def_txn_expire_wait = std::chrono::seconds(3);
constexpr auto def_resp_expected_wait = std::chrono::seconds(5);
constexpr auto def_sync_fetch_span = 100;
constexpr auto def_keepalive_interval = 32000;

constexpr auto message_header_size = 4;

enum class round_step_type {
  NewHeight = 1, // Wait til CommitTime + timeoutCommit
  NewRound = 2, // Setup new round and go to RoundStepPropose
  Propose = 3, // Did propose, gossip proposal
  Prevote = 4, // Did prevote, gossip prevotes
  PrevoteWait = 5, // Did receive any +2/3 prevotes, start timeout
  Precommit = 6, // Did precommit, gossip precommits
  PrecommitWait = 7, // Did receive any +2/3 precommits, start timeout
  Commit = 8 // Entered commit state machine
};

constexpr auto round_step_to_str(round_step_type step) {
  switch (step) {
  case round_step_type::NewHeight:
    return "NewHeight";
  case round_step_type::NewRound:
    return "NewRound";
  case round_step_type::Propose:
    return "Propose";
  case round_step_type::Prevote:
    return "Prevote";
  case round_step_type::PrevoteWait:
    return "PrevoteWait";
  case round_step_type::Precommit:
    return "Precommit";
  case round_step_type::PrecommitWait:
    return "PrecommitWait";
  case round_step_type::Commit:
    return "Commit";
  default:
    return "Unknown";
  }
}

enum class peer_status {
  up, // Connected and ready
  down, // Disconnected
  good, // Peer observed as good
  bad // Peer observed as bad
};

constexpr auto peer_status_to_str(peer_status status) {
  switch (status) {
  case peer_status::up:
    return "Up";
  case peer_status::down:
    return "Down";
  case peer_status::good:
    return "Good";
  case peer_status::bad:
    return "Bad";
  default:
    return "Unknown";
  }
}

enum reactor_id {
  Consensus = 1,
  BlockSync
};

struct envelope {
  std::string from;
  std::string to;
  bool broadcast;
  bytes message; ///< one of reactor_messages, serialized
  reactor_id id;
};
using envelope_ptr = std::shared_ptr<envelope>;

} // namespace noir::p2p

NOIR_REFLECT(noir::p2p::envelope, from, to, broadcast, message, id)
