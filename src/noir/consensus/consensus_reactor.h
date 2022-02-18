// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/consensus_state.h>
#include <noir/consensus/peer_state.h>
#include <noir/consensus/state.h>
#include <noir/consensus/store/store_test.h>
#include <noir/consensus/types.h>

namespace noir::consensus {

struct consensus_reactor {

  std::shared_ptr<consensus_state> cs_state;

  std::mutex mtx;
  std::map<node_id, peer_state> peers;
  bool wait_sync;

  // todo
  //  stateCh       *p2p.Channel
  //  dataCh        *p2p.Channel
  //  voteCh        *p2p.Channel
  //  voteSetBitsCh *p2p.Channel
  //  peerUpdates   *p2p.PeerUpdates

  consensus_reactor(std::shared_ptr<consensus_state> new_cs_state, bool new_wait_sync)
    : cs_state(std::move(new_cs_state)), wait_sync(new_wait_sync) {}

  static std::shared_ptr<consensus_reactor> new_consensus_reactor(
    std::shared_ptr<consensus_state>& new_cs_state, bool new_wait_sync) {
    auto consensus_reactor_ = std::make_shared<consensus_reactor>(new_cs_state, new_wait_sync);
    return consensus_reactor_;
  }

  void on_start() {}

  void on_stop() {}

  void GetPeerState() {}

  void broadcastNewRoundStepMessage() {}

  void broadcastNewValidBlockMessage() {}

  void broadcastHasVoteMessage() {}

  void makeRoundStepMessage() {}

  void sendNewRoundStepMessage() {}

  void gossipVotesForHeight() {}

  void handleStateMessage() {}

  void handleVoteMessage() {}

  void handleMessage() {}
};

} // namespace noir::consensus
