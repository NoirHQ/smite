// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/scope_exit.h>
#include <noir/common/thread_pool.h>
#include <noir/consensus/config.h>
#include <noir/consensus/consensus_state.h>
#include <noir/consensus/crypto.h>
#include <noir/consensus/priv_validator.h>
#include <noir/consensus/state.h>
#include <noir/consensus/types.h>

#include <appbase/application.hpp>
#include <boost/asio/steady_timer.hpp>
#include <fmt/core.h>

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
  std::unique_ptr<round_state> get_round_state();
  void set_priv_validator(const priv_validator& priv);
  void update_priv_validator_pub_key();
  void reconstruct_last_commit(state& state_);

  void on_start();

  void update_height(int64_t height);
  void update_round_step(int32_t rount, round_step_type step);
  void schedule_round_0(round_state& rs_);
  void update_to_state(state& state_);
  void new_step();

  void receive_routine(int max_steps);
  void handle_msg();

  void schedule_timeout(
    std::chrono::system_clock::duration duration_, int64_t height, int32_t round, round_step_type step);
  void tick(timeout_info_ptr ti);
  void tock(timeout_info_ptr ti);
  void handle_timeout(timeout_info_ptr ti);

  void enter_new_round(int64_t height, int32_t round);

  void enter_propose(int64_t height, int32_t round);
  bool is_proposal_complete();
  bool is_proposal(p2p::bytes address);
  void decide_proposal(int64_t height, int32_t round);

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
  std::optional<priv_validator> local_priv_validator;
  priv_validator_type local_priv_validator_type;

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
  round_state rs{};
  state local_state; // State until height-1.

  //  // privValidator pubkey, memoized for the duration of one block to avoid extra requests to HSM
  //  privValidatorPubKey crypto.PubKey
  pub_key local_priv_validator_pub_key;

  //
  //  // state changes may be triggered by: msgs from peers,
  //  // msgs from ourself, or by timeouts
  //  peerMsgQueue     chan msgInfo
  //  internalMsgQueue chan msgInfo

  //    timeoutTicker    TimeoutTicker
  channels::timeout_ticker::channel_type& timeout_ticker_channel;
  channels::timeout_ticker::channel_type::handle timeout_ticker_subscription;
  std::mutex timeout_ticker_mtx;
  std::unique_ptr<boost::asio::steady_timer> timeout_ticker_timer;
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
  int n_steps;

  //
  //  // some functions can be overwritten for testing
  //  decideProposal func(height int64, round int32)
  //  doPrevote      func(height int64, round int32)
  //  setProposal    func(proposal *types.Proposal) error
  /////-- directly implemented decide_proposal

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

consensus_state::consensus_state() : timeout_ticker_channel(appbase::app().get_channel<channels::timeout_ticker>()) {
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

  if (state_.last_block_height > 0) {
    consensus_state_->reconstruct_last_commit(state_);
  }

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

std::unique_ptr<round_state> consensus_state::get_round_state() {
  std::lock_guard<std::mutex> g(mtx);
  auto rs_copy = std::make_unique<round_state>();
  *rs_copy = rs;
  return rs_copy;
}

void consensus_state::set_priv_validator(const priv_validator& priv) {
  std::lock_guard<std::mutex> g(mtx);

  local_priv_validator = priv;

  //  switch(priv.type) {
  //  }
  local_priv_validator_type = FileSignerClient; // todo - implement FilePV

  update_priv_validator_pub_key();
}

/**
 * get private validator public key and memoizes it
 */
void consensus_state::update_priv_validator_pub_key() {
  if (!local_priv_validator.has_value())
    return;

  std::chrono::system_clock::duration timeout = cs_config.timeout_prevote;
  if (cs_config.timeout_precommit > cs_config.timeout_prevote)
    timeout = cs_config.timeout_precommit;

  // no GetPubKey retry beyond the proposal/voting in RetrySignerClient
  if (rs.step >= Precommit && local_priv_validator_type == RetrySignerClient)
    timeout = std::chrono::system_clock::duration::zero();

  auto key = local_priv_validator->get_pub_key();
  local_priv_validator_pub_key = pub_key{key};
}

void consensus_state::reconstruct_last_commit(state& state_) {
  // todo
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
void consensus_state::schedule_round_0(round_state& rs_) {
  auto sleep_duration = rs_.start_time - get_time();
  schedule_timeout(std::chrono::microseconds(sleep_duration), rs_.height, 0, NewHeight); // todo
}

/**
 * Updates consensus_state and increments height to match that of state.
 * The round becomes 0 and step becomes RoundStepNewHeight.
 */
void consensus_state::update_to_state(state& state_) {
  if ((rs.commit_round > -1) && (0 < rs.height) && (rs.height != state_.last_block_height)) {
    throw std::runtime_error(
      fmt::format("update_to_state() unexpected state height of {} but found {}", rs.height, state_.last_block_height));
  }

  if (!local_state.is_empty()) {
    if (local_state.last_block_height > 0 && local_state.last_block_height + 1 != rs.height) {
      // This might happen when someone else is mutating local_state.
      // Someone forgot to pass in state.Copy() somewhere?!
      throw std::runtime_error(fmt::format(
        "inconsistent local_state.last_block_height+1={} vs rs.height=", local_state.last_block_height + 1, rs.height));
    }
    if (local_state.last_block_height > 0 && rs.height == local_state.initial_height) {
      throw std::runtime_error(
        fmt::format("inconsistent local_state.last_block_height={}, expected 0 for initial height {}",
          local_state.last_block_height, local_state.initial_height));
    }

    // If state_ isn't further out than local_state, just ignore.
    // This happens when SwitchToConsensus() is called in the reactor.
    // We don't want to reset e.g. the Votes, but we still want to
    // signal the new round step, because other services (eg. txNotifier)
    // depend on having an up-to-date peer state!
    if (state_.last_block_height <= local_state.last_block_height) {
      dlog(fmt::format("ignoring update_to_state(): new_height={} old_height={}", state_.last_block_height + 1,
        local_state.last_block_height + 1));
      new_step();
      return;
    }
  }

  // Reset fields based on state.
  rs.validators = std::make_shared<validator_set>(state_.validators);

  if (state_.last_block_height == 0) {
    // very first commit should be empty
    rs.last_commit = {};
  } else if (rs.commit_round > -1 && rs.votes != nullptr) {
    // Use rs.votes
    if (!rs.votes->precommits(rs.commit_round)->has_two_thirds_majority()) {
      throw std::runtime_error(fmt::format("wanted to form a commit, but precommits (H/R: {}/{}) didn't have 2/3+",
        state_.last_block_height, rs.commit_round));
    }
    rs.last_commit = rs.votes->precommits(rs.commit_round);
  } else if (!rs.last_commit.has_value()) {
    // NOTE: when Tendermint starts, it has no votes. reconstructLastCommit
    // must be called to reconstruct LastCommit from SeenCommit.
    throw std::runtime_error(
      fmt::format("last commit cannot be empty after initial block (height={})", state_.last_block_height + 1));
  }

  // Next desired block height
  auto height = state_.last_block_height + 1;
  if (height == 1)
    height = state_.initial_height;

  // RoundState fields
  update_height(height);
  update_round_step(0, NewHeight);

  // todo
  //  if (rs.commit_time == 0)
  //    rs.start_time = config.;
  //  else
  //    rs.start_time =

  rs.proposal = {};
  rs.proposal_block = {};
  rs.proposal_block_parts = {};
  rs.locked_round = -1;
  rs.locked_block = nullptr;
  rs.locked_block_parts = nullptr;

  rs.valid_round = -1;
  rs.valid_block = {};
  rs.valid_block_parts = {};
  rs.votes = height_vote_set::new_height_vote_set(state_.chain_id, height, state_.validators);
  rs.commit_round = -1;
  rs.last_validators = std::make_shared<validator_set>(state_.last_validators);
  rs.triggered_timeout_precommit = false;

  local_state = state_;

  // Finally, broadcast RoundState
  new_step();
}

void consensus_state::new_step() {
  // todo
  n_steps++;
}

/**
 * receiveRoutine handles messages which may cause state transitions.
 * it's argument (n) is the number of messages to process before exiting - use 0 to run forever
 * It keeps the RoundState and is the only thing that updates it.
 * Updates (state transitions) happen on timeouts, complete proposals, and 2/3 majorities.
 * State must be locked before any internal state is updated.
 */
void consensus_state::receive_routine(int max_steps) {}

void consensus_state::handle_msg() {
  // todo
}

void consensus_state::schedule_timeout(
  std::chrono::system_clock::duration duration_, int64_t height, int32_t round, round_step_type step) {
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
      // return; // by commenting this line out, we'll process the last tock
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
  if ((rs.height != height) || (round < rs.round) || (rs.round == round && rs.step != NewHeight)) {
    dlog("entering new round with invalid args: height=${height} round={round} step=${step}",
      ("height", rs.height)("round", rs.round)("step", rs.step));
    return;
  }

  auto now = get_time();
  if (rs.start_time > now) {
    dlog("need to set a buffer and log message here for sanity");
  }
  dlog(fmt::format("entering new round: current={}/{}/{}", rs.height, rs.round, rs.step));

  // increment validators if necessary
  auto validators = rs.validators;
  if (rs.round < round) {
    validators->increment_proposer_priority(round - rs.round); // todo - safe sub
  }

  // Setup new round
  // we don't fire newStep for this step,
  // but we fire an event, so update the round step first
  update_round_step(round, NewRound);
  rs.validators = validators;
  if (round == 0) {
    // We've already reset these upon new height,
    // and meanwhile we might have received a proposal
    // for round 0.
  } else {
    dlog("resetting proposal info");
    rs.proposal = {};
    rs.proposal_block = {};
    rs.proposal_block_parts = {};
  }

  rs.votes->set_round(round + 1); // todo - safe math
  rs.triggered_timeout_precommit = false;

  // event bus // todo?
  // metrics // todo?

  // wait for tx? // todo?
  enter_propose(height, round);
}

void consensus_state::enter_propose(int64_t height, int32_t round) {
  if (rs.height != height || round < rs.round || (rs.round == round && Propose <= rs.step)) {
    dlog(fmt::format("entering propose step with invalid args: {}/{}/{}", rs.height, rs.round, rs.step));
    return;
  }
  dlog(fmt::format("entering propose step: {}/{}/{}", rs.height, rs.round, rs.step));

  auto defer = fc::make_scoped_exit([this, height, round]() {
    update_round_step(round, Propose);
    new_step();
    if (is_proposal_complete())
      enter_prevote(height, rs.round);
  });

  // If we don't get the proposal and all block parts quick enough, enter_prevote
  schedule_timeout(cs_config.propose(round), height, round, Propose);

  // Nothing more to do if we are not a validator
  if (!local_priv_validator.has_value()) {
    dlog("node is not a validator");
    return;
  }
  dlog("node is a validator");

  if (local_priv_validator_pub_key.empty()) {
    // If this node is a validator & proposer in the current round, it will
    // miss the opportunity to create a block.
    elog("propose step; empty priv_validator_pub_key is not set");
    return;
  }

  auto address = local_priv_validator_pub_key.address();

  // If not a validator, we are done
  if (!rs.validators->has_address(address)) {
    dlog(fmt::format("node is not a validator: addr_size={}", address.size()));
    return;
  }

  if (is_proposal(address)) {
    dlog("propose step; our turn to propose");
    decide_proposal(height, round);
  } else {
    dlog("propose step; not our turn to propose");
  }
}

/**
 * returns true if the proposal block is complete, if POL_round was proposed, and we have 2/3+ prevotes
 */
bool consensus_state::is_proposal_complete() {
  if (!rs.proposal.has_value() || !rs.proposal_block.has_value())
    return false;
  // We have the proposal. If there is a POL_round, make sure we have prevotes from it.
  if (rs.proposal->pol_round < 0)
    return true;
  // if this is false the proposer is lying or we haven't received the POL yet
  return rs.votes->prevotes(rs.proposal->pol_round)->has_two_thirds_majority();
}

bool consensus_state::is_proposal(p2p::bytes address) {
  return rs.validators->get_proposer()->address == address;
}

void consensus_state::decide_proposal(int64_t height, int32_t round) {
  std::optional<block> block_;
  std::optional<part_set> block_parts_;

  // Decide on a block
  if (rs.valid_block.has_value()) {
    // If there is valid block, choose that
    block_ = rs.valid_block;
    block_parts_ = rs.valid_block_parts;
  } else {
    // Create a new proposal block from state/txs from the mempool
    //    block_ = create_proposal_block();
    if (!local_priv_validator.has_value())
      throw std::runtime_error("attempted to create proposal block with empty priv_validator");

    commit commit_;
    std::vector<vote> votes_;
    if (rs.height == local_state.initial_height) {
      // We are creating a proposal for the first block
      std::vector<commit_sig> commit_sigs;
      commit_ = commit::new_commit(0, 0, p2p::block_id{}, commit_sigs);
    } else if (rs.last_commit->has_two_thirds_majority()) {
      // Make the commit from last_commit
      commit_ = rs.last_commit->make_commit();
      votes_ = rs.last_commit->votes;
    } else {
      elog("propose step; cannot propose anyting without commit for the previous block");
      return;
    }
    auto proposer_addr = local_priv_validator_pub_key.address();

    // block_exec.create_proposal_block // todo - requires interface to mempool?

    if (!block_.has_value()) {
      wlog("MUST CONNECT TO MEMPOOL IN ORDER TO RETRIEVE SOME BLOCKS"); // todo - remove once mempool is ready
      return;
    }
  }

  // Flush the WAL
  // todo

  // Make proposal
  auto prop_block_id = p2p::block_id{block_->get_hash(), block_parts_->header()};
  auto proposal_ = proposal::new_proposal(height, round, rs.valid_round, prop_block_id);

  // todo - sign and send internal msg queue
}

void consensus_state::enter_prevote(int64_t height, int32_t round) {}

void consensus_state::enter_prevote_wait(int64_t height, int32_t round) {}

void consensus_state::enter_precommit(int64_t height, int32_t round) {}

void consensus_state::enter_precommit_wait(int64_t height, int32_t round) {}

void consensus_state::enter_commit(int64_t height, int32_t round) {}

} // namespace noir::consensus
