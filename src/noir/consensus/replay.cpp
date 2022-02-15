// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/scope_exit.h>
#include <noir/consensus/consensus_state.h>
#include <noir/consensus/wal.h>
#include <fmt/format.h>

namespace noir::consensus {
using namespace noir::p2p;

// Replay only those messages since the last block.  `timeoutRoutine` should
// run concurrently to read off tickChan.
bool consensus_state::catchup_replay(int64_t cs_height) {
  // Set replayMode to true so we don't log signing errors.
  replay_mode = true;
  auto defer = make_scoped_exit([this]() { replay_mode = false; });

  // Ensure that #ENDHEIGHT for this height doesn't exist.
  // NOTE: This is just a sanity check. As far as we know things work fine
  // without it, and Handshake could reuse State if it weren't for
  // this check (since we can crash after writing #ENDHEIGHT).
  //
  // Ignore data corruption errors since this is a sanity check.
  {
    bool found;
    auto dec = wal_->search_for_end_height(cs_height, wal_search_options{.ignore_data_corruption_errors = true}, found);
    if (dec) {
      // dec->close();
      return false;
    }
    if (found) {
      elog(fmt::format("wal should not contain #ENDHEIGHT {}", cs_height));
      return false;
    }
  }

  // Search for last height marker.
  //
  // Ignore data corruption errors in previous heights because we only care about last height
  if (cs_height < local_state.initial_height) {
    elog(fmt::format("cannot replay height {}, below initial height {}", cs_height, local_state.initial_height));
    return false;
  }
  auto end_height = (cs_height == local_state.initial_height) ? 0 : cs_height - 1;

  bool found;
  auto dec = wal_->search_for_end_height(end_height, wal_search_options{.ignore_data_corruption_errors = true}, found);
  // handle EOF?
  if (!found) {
    elog(fmt::format("cannot replay height {}. WAL does not contain #ENDHEIGHT for {}", cs_height, end_height));
    return false;
  }
  auto defer_close = make_scoped_exit([&dec]() {
    // dec->close();
  });

  timed_wal_message tmsg{};

  auto ret = dec->decode(tmsg);
  while (ret == wal_decoder::result::success) {
    // NOTE: since the priv key is set when the msgs are received
    // it will attempt to eg double sign but we can just ignore it
    // since the votes will be replayed and we'll get to the next step
    if (!read_replay_message(tmsg)) {
      return false;
    }
    ret = dec->decode(tmsg);
  }
  check(ret == wal_decoder::result::eof, fmt::format("the WAL repair failed={}", static_cast<int32_t>(ret)));
  ilog("Replay:Done");
  return true;
}

struct wal_consensus_msg_replay_handler {
  std::shared_ptr<consensus_state> cs;
  const noir::p2p::node_id& peer_id;

  explicit wal_consensus_msg_replay_handler(
    const std::shared_ptr<consensus_state>& cs_, const noir::p2p::node_id& peer_id)
    : cs(cs_), peer_id(peer_id) {}

  void operator()(const std::shared_ptr<p2p::proposal_message>& msg) {
    if (!msg.get()) {
      return;
    }
    ilog("replay: Proposal");
  }

  void operator()(const p2p::block_part_message& msg) {
    ilog("replay: BlockPart");
  }

  void operator()(const p2p::vote_message& msg) {
    ilog("replay: Vote");
  }

  void operator()(auto& msg) {
    elog("replay: Unknown consensus_message type");
  }
};

struct wal_replay_handler {
  std::shared_ptr<consensus_state> cs;
  explicit wal_replay_handler(const std::shared_ptr<consensus_state>& cs_): cs(cs_) {}

  bool operator()(const end_height_message& msg) {
    // Skip meta messages which exist for demarcating boundaries.
    return true;
  }
  bool operator()(const round_state::event_data& msg) {
    // for logging
    ilog(fmt::format("Replay: New Step height={} round={} step={}", msg.height, msg.round, msg.step));
    // TODO: handle step_sub
    return true;
  }
  bool operator()(const msg_info& msg) {
    auto peer_id = msg.peer_id;
    if (peer_id == "") {
      peer_id = "local";
    }
    std::visit(wal_consensus_msg_replay_handler{cs, peer_id}, msg.msg);
    cs->handle_msg(); // TODO: connect msg
    return true;
  }
  bool operator()(const timeout_info& msg) {
    ilog(fmt::format("Replay: {}/{}/{}, timeout={}", msg.height, msg.round, msg.step, msg.duration_.count()));
    cs->handle_timeout(std::make_shared<timeout_info>(msg));
    return true;
  }
  bool operator()(auto& msg) {
    elog("replay: Unknown TimedWALMessage type");
    return false;
  }
};

// Functionality to replay blocks and messages on recovery from a crash.
// There are two general failure scenarios:
//
//  1. failure during consensus
//  2. failure while applying the block
//
// The former is handled by the WAL, the latter by the proxyApp Handshake on
// restart, which ultimately hands off the work to the WAL.

//-----------------------------------------
// 1. Recover from failure during consensus
// (by replaying messages from the WAL)
//-----------------------------------------

// Unmarshal and apply a single message to the consensus state as if it were
// received in receiveRoutine.  Lines that start with "#" are ignored.
// NOTE: receiveRoutine should not be running.
bool consensus_state::read_replay_message(const timed_wal_message& msg) {
  return std::visit(wal_replay_handler{shared_from_this()}, msg.msg.msg);
}

} // namespace noir::consensus
