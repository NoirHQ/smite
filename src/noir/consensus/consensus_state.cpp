// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/scope_exit.h>
#include <noir/consensus/consensus_state.h>

#include <appbase/application.hpp>
#include <boost/asio/steady_timer.hpp>
#include <fmt/core.h>

namespace noir::consensus {

struct message_handler : public fc::visitor<void> {
  std::shared_ptr<consensus_state> cs;

  explicit message_handler(const std::shared_ptr<consensus_state>& cs_) : cs(cs_) {}

  void operator()(p2p::proposal_message& msg) {
    std::lock_guard<std::mutex> g(cs->mtx);
    // will not cause transition.
    // once proposal is set, we can receive block parts
    cs->set_proposal(msg);
  }

  void operator()(p2p::block_part_message& msg) {
    std::lock_guard<std::mutex> g(cs->mtx);
    // if the proposal is complete, we'll enter_prevote or try_finalize_commit
    auto added = cs->add_proposal_block_part(msg, "");
    if (msg.round != cs->rs.round) {
      dlog(fmt::format("received block part from wrong round: height={} cs_round={} block_round={}", cs->rs.height,
        cs->rs.round, msg.round));
    }
  }

  void operator()(p2p::vote_message& msg) {
    std::lock_guard<std::mutex> g(cs->mtx);
    // attempt to add the vote and dupeout the validator if its a duplicate signature
    // if the vote gives us a 2/3-any or 2/3-one, we transition
    cs->try_add_vote(msg, "");
  }
};

consensus_state::consensus_state()
  : timeout_ticker_channel(appbase::app().get_channel<channels::timeout_ticker>()),
    internal_mq_channel(appbase::app().get_channel<channels::internal_message_queue>()),
    peer_mq_channel(appbase::app().get_channel<channels::peer_message_queue>()) {
  timeout_ticker_subscription = appbase::app().get_channel<channels::timeout_ticker>().subscribe(
    std::bind(&consensus_state::tock, shared_from_this(), std::placeholders::_1));
  internal_mq_subscription = appbase::app().get_channel<channels::internal_message_queue>().subscribe(
    std::bind(&consensus_state::receive_routine, shared_from_this(), std::placeholders::_1));
  peer_mq_subscription = appbase::app().get_channel<channels::peer_message_queue>().subscribe(
    std::bind(&consensus_state::receive_routine, shared_from_this(), std::placeholders::_1));

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

  // todo - notify consensus_reactor about rs
}

/**
 * receiveRoutine handles messages which may cause state transitions.
 * it's argument (n) is the number of messages to process before exiting - use 0 to run forever
 * It keeps the RoundState and is the only thing that updates it.
 * Updates (state transitions) happen on timeouts, complete proposals, and 2/3 majorities.
 * State must be locked before any internal state is updated.
 */
void consensus_state::receive_routine(msg_info_ptr mi) {
  message_handler m(shared_from_this());
  std::visit(m, mi->msg);
}

void consensus_state::handle_msg() {
  // not needed for noir as message_handler handles msg
  // todo - remove this function
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

  auto defer = make_scoped_exit([this, height, round]() {
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
      elog("propose step; cannot propose anything without commit for the previous block");
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
  // proposal sign - todo requires filePV, which requires querying for late saved file state
  //               - for now assume it is signed right away
  // if (local_priv_validator->sign_proposal()) {
  if (true) {
    // proposal_.signature = p.signature;

    // Send proposal and block_parts
    internal_mq_channel.publish(appbase::priority::medium, std::make_shared<msg_info>(msg_info{proposal_, ""}));

    for (auto i = 0; i < block_parts_->total; i++) {
      auto part_ = block_parts_->get_part(i);
      auto msg = p2p::block_part_message{rs.height, rs.round, part_.index, part_.bytes_, part_.proof};
      internal_mq_channel.publish(appbase::priority::medium, std::make_shared<msg_info>(msg_info{proposal_, ""}));
    }
    dlog(fmt::format("signed proposal: height={} round={}", height, round));
  } else {
    elog(fmt::format("propose step; failed signing proposal: height={} round={}", height, round));
  }
}

void consensus_state::enter_prevote(int64_t height, int32_t round) {}

void consensus_state::enter_prevote_wait(int64_t height, int32_t round) {}

void consensus_state::enter_precommit(int64_t height, int32_t round) {}

void consensus_state::enter_precommit_wait(int64_t height, int32_t round) {}

void consensus_state::enter_commit(int64_t height, int32_t round) {}

void consensus_state::set_proposal(p2p::proposal_message& msg) {
  if (!rs.proposal.has_value()) {
    dlog("set_proposal; already have one");
    return; // Already have one
  }

  // Does not apply
  if (msg.height != rs.height || msg.round != rs.round) {
    dlog("set_proposal; does not apply");
    return;
  }

  // Verify POLRound, which must be -1 or in range [0, proposal.Round).
  if (msg.pol_round < -1 || (msg.pol_round >= 0 && msg.pol_round >= msg.round)) {
    dlog("set_proposal; error invalid proposal POL round");
    return;
  }

  // Verify signature // todo
  // if !cs.Validators.GetProposer().PubKey.VerifySignature(

  // msg.sig = ;
  rs.proposal = msg;
  // We don't update cs.ProposalBlockParts if it is already set.
  // This happens if we're already in cstypes.RoundStepCommit or if there is a valid block in the current round.
  if (!rs.proposal_block_parts.has_value()) {
    rs.proposal_block_parts = part_set::new_part_set_from_header(msg.block_id_.parts);
  }

  ilog(fmt::format("received proposal; {}", msg.type));
}

/**
 * Asynchronously triggers either enterPrevote (before we timeout of propose) or tryFinalizeCommit, once we have full
 * block NOTE: block may be invalid
 */
bool consensus_state::add_proposal_block_part(p2p::block_part_message& msg, p2p::node_id peer_id) {
  auto height_ = msg.height;
  auto round_ = msg.round;
  auto part_ = part{msg.index, msg.bytes_};

  // Blocks might be reused, so round mismatch is OK
  if (rs.height != height_) {
    dlog(fmt::format("received block_part from wrong height: height={} round={}", height_, round_));
    return false;
  }

  // We are not expecting a block part
  if (!rs.proposal_block_parts.has_value()) {
    // NOTE: this can happen when we've gone to a higher round and
    // then receive parts from the previous round - not necessarily a bad peer.
    dlog(fmt::format("received block_part when we are not expecting any: height={} round={}", height_, round_));
    return false;
  }

  auto added = rs.proposal_block_parts->add_part(part_);

  if (rs.proposal_block_parts->byte_size > local_state.consensus_params.block.max_bytes) {
    elog(fmt::format("total size of proposal block parts exceeds maximum block bytes ({} > {})",
      rs.proposal_block_parts->byte_size, local_state.consensus_params.block.max_bytes));
    return added;
  }
  if (added && rs.proposal_block_parts->is_complete()) {
    // todo - implement; it's very long
  }
  return added;
}

/**
 * Attempt to add vote. If it's a duplicate signature, dupeout? the validator
 */
bool consensus_state::try_add_vote(p2p::vote_message& msg, p2p::node_id peer_id) {
  auto added = add_vote(msg, peer_id);

  // todo - implement; it's very long
}

bool consensus_state::add_vote(p2p::vote_message& msg, p2p::node_id peer_id) {
  dlog(fmt::format(
    "adding vote: height={} type={} index={} cs_height={}", msg.height, msg.type, msg.validator_index, rs.height));

  // todo - implement; it's very long
}

} // namespace noir::consensus
