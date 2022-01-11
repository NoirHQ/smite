// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma
#include <noir/consensus/consensus_state.h>
#include <noir/consensus/state.h>
#include <noir/consensus/types.h>

namespace noir::consensus {

class consensus_reactor : boost::noncopyable {
public:
  consensus_reactor();

  static std::unique_ptr<consensus_reactor> new_consensus_reactor(state& prev_state);

  void on_start();

  void on_stop();

  void GetPeerState();

  void broadcastNewRoundStepMessage();

  void broadcastNewValidBlockMessage();

  void broadcastHasVoteMessage();

  void makeRoundStepMessage();

  void sendNewRoundStepMessage();

  void gossipVotesForHeight();

  void handleStateMessage();

  void handleVoteMessage();

  void handleMessage();

private:
  //  state    *State
  std::unique_ptr<consensus_state> local_state;

  //  eventBus *types.EventBus
  //  Metrics  *Metrics
  //
  //  mtx      tmsync.RWMutex
  //  peers    map[types.NodeID]*PeerState
  //  waitSync bool
  //
  //  stateCh       *p2p.Channel
  //  dataCh        *p2p.Channel
  //  voteCh        *p2p.Channel
  //  voteSetBitsCh *p2p.Channel
  //  peerUpdates   *p2p.PeerUpdates
};

std::unique_ptr<consensus_reactor> consensus_reactor::new_consensus_reactor(state& prev_state) {
  auto consensus_reactor_ = std::make_unique<consensus_reactor>();

  consensus_reactor_->local_state = consensus_state::new_state(prev_state);

  return consensus_reactor_;
}

} // namespace noir::consensus
