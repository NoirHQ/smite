// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/consensus_state.h>
#include <noir/consensus/state.h>
#include <noir/consensus/types.h>

namespace noir::consensus {

/**
 * Handles execution of the consensus algorithm.
 * It processes votes and proposals, and upon reaching agreement,
 * commits blocks to the chain and executes them against the application.
 * The internal state machine receives input from peers, the internal validator, and from a timer.
 */
struct consensus_state {

  static std::unique_ptr<consensus_state> new_state(state& state_);

  state get_state();
  int64_t get_last_height();
  round_state get_round_state();

  void on_start();

  void update_height(int64_t height);
  void update_round_step(int32_t rount, round_step_type step);
  void schedule_round_0(round_state& rs);
  void update_to_state(state& state_);

  void receive_routine(int max_steps);
  void handle_msg();
  void handle_timeout();

//  // config details
//  config            *cfg.ConsensusConfig
//  privValidator     types.PrivValidator // for signing votes
//  privValidatorType types.PrivValidatorType
//
//  // store blocks and commits
//  blockStore sm.BlockStore
//
//  // create and execute blocks
//  blockExec *sm.BlockExecutor
//
//  // notify us if txs are available
//  txNotifier txNotifier
//
//  // add evidence to the pool
//  // when it's detected
//  evpool evidencePool
//

  // internal state
//  mtx tmsync.RWMutex
//  cstypes.RoundState
//  state sm.State // State until height-1.
  std::mutex mtx;
  round_state rs;
  state local_state; // State until height-1.
//  // privValidator pubkey, memoized for the duration of one block
//  // to avoid extra requests to HSM
//  privValidatorPubKey crypto.PubKey
//
//  // state changes may be triggered by: msgs from peers,
//  // msgs from ourself, or by timeouts
//  peerMsgQueue     chan msgInfo
//  internalMsgQueue chan msgInfo
//    timeoutTicker    TimeoutTicker
//
//  // information about about added votes and block parts are written on this channel
//  // so statistics can be computed by reactor
//  statsMsgQueue chan msgInfo
//
//  // we use eventBus to trigger msg broadcasts in the reactor,
//  // and to notify external subscribers, eg. through a websocket
//  eventBus *types.EventBus
//
//  // a Write-Ahead Log ensures we can recover from any kind of crash
//  // and helps us avoid signing conflicting votes
//  wal          WAL
//    replayMode   bool // so we don't log signing errors during replay
//  doWALCatchup bool // determines if we even try to do the catchup
//
//  // for tests where we want to limit the number of transitions the state makes
//  nSteps int
//
//  // some functions can be overwritten for testing
//  decideProposal func(height int64, round int32)
//  doPrevote      func(height int64, round int32)
//  setProposal    func(proposal *types.Proposal) error
//
//  // closed when we finish shutting down
//  done chan struct{}
//
//  // synchronous pubsub between consensus state and reactor.
//  // state only emits EventNewRoundStep and EventVote
//  evsw tmevents.EventSwitch
//
//  // for reporting metrics
//  metrics *Metrics
//
//  // wait the channel event happening for shutting down the state gracefully
//  onStopCh chan *cstypes.RoundState
};

std::unique_ptr<consensus_state> consensus_state::new_state(state& state_) {
  auto consensus_state_ = std::make_unique<consensus_state>();

  consensus_state_->update_to_state(state_);

  return consensus_state_;
}

state consensus_state::get_state() {
  std::lock_guard<std::mutex> g(mtx);
  return local_state;
}

int64_t consensus_state::get_last_height() {
  std::lock_guard<std::mutex> g(mtx);
  return rs.height - 1;
}

round_state consensus_state::get_round_state() {
  std::lock_guard<std::mutex> g(mtx);
  return rs;
}

void consensus_state::on_start() {
  // todo
}

void consensus_state::update_height(int64_t height) {
  rs.height = height;
}

void consensus_state::update_round_step(int32_t round, round_step_type step) {
  rs.round = round;
  rs.step = step;
}

/**
 * enterNewRound(height, 0) at StartTime.
 */
void consensus_state::schedule_round_0(round_state& rs) {
//  sleepDuration := rs.StartTime.Sub(tmtime.Now())
//  cs.scheduleTimeout(sleepDuration, rs.Height, 0, cstypes.RoundStepNewHeight)
}

/**
 * Updates consensus_state and increments height to match that of state.
 * The round becomes 0 and step becomes RoundStepNewHeight.
 */
void consensus_state::update_to_state(state& state_) {
  // todo
}

/**
 * receiveRoutine handles messages which may cause state transitions.
 * it's argument (n) is the number of messages to process before exiting - use 0 to run forever
 * It keeps the RoundState and is the only thing that updates it.
 * Updates (state transitions) happen on timeouts, complete proposals, and 2/3 majorities.
 * State must be locked before any internal state is updated.
 */
void consensus_state::receive_routine(int max_steps) {

}

void consensus_state::handle_msg() {
  // todo
}

void consensus_state::handle_timeout() {

}

} // namespace noir::consensus
