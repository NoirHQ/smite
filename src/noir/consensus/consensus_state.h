// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/config.h>
#include <noir/consensus/consensus_state.h>
#include <noir/consensus/state.h>
#include <noir/consensus/types.h>
#include <noir/common/thread_pool.h>

#include <appbase/application.hpp>
#include <boost/asio/steady_timer.hpp>

namespace noir::consensus {

/**
 * Handles execution of the consensus algorithm.
 * It processes votes and proposals, and upon reaching agreement,
 * commits blocks to the chain and executes them against the application.
 * The internal state machine receives input from peers, the internal validator, and from a timer.
 */
struct consensus_state {
  consensus_state();

  static std::unique_ptr<consensus_state> new_state(const consensus_config& cs_config_, state& state_);

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

  void
  schedule_timeout(std::chrono::system_clock::duration duration_, int64_t height, int32_t round, round_step_type step);
  void tick(timeout_info_ptr ti);
  void tock(timeout_info_ptr ti);
  void handle_timeout(timeout_info_ptr ti);

  void enter_new_round(int64_t height, int32_t round);
  void enter_propose(int64_t height, int32_t round);
  void enter_prevote(int64_t height, int32_t round);
  void enter_prevote_wait(int64_t height, int32_t round);
  void enter_precommit(int64_t height, int32_t round);
  void enter_precommit_wait(int64_t height, int32_t round);
  void enter_commit(int64_t height, int32_t round);

//  // config details
//  config            *cfg.ConsensusConfig
  consensus_config cs_config;

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
  channels::timeout_ticker::channel_type& timeout_ticker_channel;
  channels::timeout_ticker::channel_type::handle timeout_ticker_subscription;
  std::mutex timeout_ticker_mtx;
  unique_ptr<boost::asio::steady_timer> timeout_ticker_timer;
  uint16_t thread_pool_size = 2;
  std::optional<named_thread_pool> thread_pool;
  timeout_info_ptr old_ti;

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

consensus_state::consensus_state()
  : timeout_ticker_channel(appbase::app().get_channel<channels::timeout_ticker>()) {
  timeout_ticker_subscription = appbase::app().get_channel<channels::timeout_ticker>().subscribe(
    std::bind(&consensus_state::tock, this, std::placeholders::_1));

  thread_pool.emplace("consensus", thread_pool_size);
  {
    std::lock_guard<std::mutex> g(timeout_ticker_mtx);
    timeout_ticker_timer.reset(new boost::asio::steady_timer(thread_pool->get_executor()));
  }
  old_ti = std::make_shared<timeout_info>(timeout_info{});
}

std::unique_ptr<consensus_state> consensus_state::new_state(const consensus_config& cs_config_, state& state_) {
  auto consensus_state_ = std::make_unique<consensus_state>();
  consensus_state_->cs_config = cs_config_;

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

void consensus_state::schedule_timeout(std::chrono::system_clock::duration duration_, int64_t height, int32_t round,
  round_step_type step) {
  tick(std::make_shared<timeout_info>(timeout_info{duration_, height, round, step}));
}

void consensus_state::tick(timeout_info_ptr ti) {
  std::lock_guard<std::mutex> g(timeout_ticker_mtx);
  elog("received tick: old_ti=${old_ti}, new_ti=${new_ti}", ("old_ti", old_ti)("new_ti", ti));

  // ignore tickers for old height/round/step
  if (ti->height < old_ti->height) {
    return;
  } else if (ti->height == old_ti->height) {
    if (ti->round < old_ti->round) {
      return;
    } else if (ti->round == old_ti->round) {
      if ((old_ti->step > 0) && (ti->step <= old_ti->step)) {
        return;
      }
    }
  }

  timeout_ticker_timer->cancel();

  // update timeoutInfo and reset timer
  old_ti = ti;
  timeout_ticker_timer->expires_from_now(ti->duration_);
  timeout_ticker_timer->async_wait([this, ti](boost::system::error_code ec) {
    if (ec) {
      wlog("consensus_state timeout error: ${m}", ("m", ec.message()));
      //return; // by commenting this line out, we'll process the last tock
    }
//    timeout_ticker_channel.publish(appbase::priority::medium, ti); // -> tock
    tock(ti); // directly call (temporary workaround) // todo - use channel instead
  });
}

void consensus_state::tock(timeout_info_ptr ti) {
  ilog("Timed out: ti=${ti}", ("ti", ti));
  handle_timeout(ti);
}

void consensus_state::handle_timeout(timeout_info_ptr ti) {
  std::lock_guard<std::mutex> g(mtx);
  elog("Received tock: tock=${ti} timeout=${timeout} height=${height} round={round} step=${step}",
    ("ti", ti)("timeout", ti->duration_)("height", ti->height)("round", ti->round)("step", ti->step));

  // timeouts must be for current height, round, step
  if ((ti->height != rs.height) || (ti->round < rs.round) || (ti->round == rs.round && ti->step < rs.step)) {
    dlog("Ignoring tock because we are ahead: height=${height} round={round} step=${step}",
      ("height", ti->height)("round", ti->round)("step", ti->step));
    return;
  }

  switch (ti->step) {
    case NewHeight:
      enter_new_round(ti->height, 0);
      break;
    case NewRound:
      enter_propose(ti->height, 0);
      break;
    case Propose:
      enter_prevote(ti->height, ti->round);
      break;
    case PrevoteWait:
      enter_precommit(ti->height, ti->round);
      break;
    case PrecommitWait:
      enter_precommit(ti->height, ti->round);
      enter_new_round(ti->height, ti->round + 1);
      break;
    default:
      throw std::runtime_error("invalid timeout step");
  }
}

void consensus_state::enter_new_round(int64_t height, int32_t round) {

}

void consensus_state::enter_propose(int64_t height, int32_t round) {

}

void consensus_state::enter_prevote(int64_t height, int32_t round) {

}

void consensus_state::enter_prevote_wait(int64_t height, int32_t round) {

}

void consensus_state::enter_precommit(int64_t height, int32_t round) {

}

void consensus_state::enter_precommit_wait(int64_t height, int32_t round) {

}

void consensus_state::enter_commit(int64_t height, int32_t round) {

}

} // namespace noir::consensus
