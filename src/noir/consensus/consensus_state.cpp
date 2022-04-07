// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/helper/go.h>
#include <noir/consensus/consensus_state.h>
#include <noir/consensus/types/canonical.h>
#include <noir/core/codec.h>

#include <appbase/application.hpp>
#include <boost/asio/steady_timer.hpp>
#include <fmt/core.h>
#include <utility>

namespace noir::consensus {
namespace fs = std::filesystem;
using p2p::round_step_to_str;
using p2p::round_step_type;

struct message_handler {
  std::shared_ptr<consensus_state> cs;

  explicit message_handler(std::shared_ptr<consensus_state> cs_): cs(std::move(cs_)) {}

  void operator()(p2p::proposal_message& msg) {
    std::scoped_lock g(cs->mtx);
    // will not cause transition.
    // once proposal is set, we can receive block parts
    cs->set_proposal(msg);
  }

  void operator()(p2p::block_part_message& msg) {
    std::scoped_lock g(cs->mtx);
    // if the proposal is complete, we'll enter_prevote or try_finalize_commit
    auto added = cs->add_proposal_block_part(msg, node_id{});
    if (msg.round != cs->rs.round) {
      dlog(fmt::format("received block part from wrong round: height={} cs_round={} block_round={}", cs->rs.height,
        cs->rs.round, msg.round));
    }
  }

  void operator()(p2p::vote_message& msg) {
    std::scoped_lock g(cs->mtx);
    // attempt to add the vote and dupeout the validator if its a duplicate signature
    // if the vote gives us a 2/3-any or 2/3-one, we transition
    cs->try_add_vote(msg, node_id{});
  }
};

consensus_state::consensus_state(appbase::application& app, const std::shared_ptr<events::event_bus>& event_bus_)
  : timeout_ticker_channel(app.get_channel<plugin_interface::channels::timeout_ticker>()),
    internal_mq_channel(app.get_channel<plugin_interface::channels::internal_message_queue>()),
    event_switch_mq_channel(app.get_channel<plugin_interface::egress::channels::event_switch_message_queue>()),
    event_bus_(event_bus_), wal_(std::make_unique<nil_wal>()) {
  timeout_ticker_subscription = app.get_channel<plugin_interface::channels::timeout_ticker>().subscribe(
    std::bind(&consensus_state::tock, this, std::placeholders::_1));
  internal_mq_subscription = app.get_channel<plugin_interface::channels::internal_message_queue>().subscribe(
    std::bind(&consensus_state::receive_routine, this, std::placeholders::_1));

  thread_pool.emplace("consensus", thread_pool_size);
  {
    std::scoped_lock g(timeout_ticker_mtx);
    timeout_ticker_timer.reset(new boost::asio::steady_timer(thread_pool->get_executor()));
  }
  old_ti = std::make_shared<timeout_info>(timeout_info{});
}

std::shared_ptr<consensus_state> consensus_state::new_state(appbase::application& app,
  const consensus_config& cs_config_, state& state_, const std::shared_ptr<block_executor>& block_exec_,
  const std::shared_ptr<block_store>& new_block_store, const std::shared_ptr<events::event_bus>& event_bus_) {
  auto consensus_state_ = std::make_shared<consensus_state>(app, event_bus_);
  consensus_state_->cs_config = cs_config_;
  consensus_state_->block_exec = block_exec_;
  consensus_state_->block_store_ = new_block_store;
  consensus_state_->do_wal_catchup = true;

  if (state_.last_block_height > 0) {
    consensus_state_->reconstruct_last_commit(state_);
  }

  consensus_state_->update_to_state(state_);

  return consensus_state_;
}

state consensus_state::get_state() {
  std::scoped_lock g(mtx);
  return local_state;
}

int64_t consensus_state::get_last_height() {
  std::scoped_lock g(mtx);
  return rs.height - 1;
}

std::shared_ptr<round_state> consensus_state::get_round_state() {
  std::scoped_lock g(mtx);
  auto rs_copy = std::make_shared<round_state>();
  *rs_copy = rs;
  return rs_copy;
}

void consensus_state::set_priv_validator(const std::shared_ptr<priv_validator>& priv) {
  std::scoped_lock g(mtx);

  local_priv_validator = priv;
  local_priv_validator_type = priv->get_type(); // todo - implement FilePV

  update_priv_validator_pub_key();
}

/**
 * get private validator public key and memoizes it
 */
void consensus_state::update_priv_validator_pub_key() {
  if (!local_priv_validator)
    return;

  std::chrono::system_clock::duration timeout = cs_config.timeout_prevote;
  if (cs_config.timeout_precommit > cs_config.timeout_prevote)
    timeout = cs_config.timeout_precommit;

  // no GetPubKey retry beyond the proposal/voting in RetrySignerClient
  if (rs.step >= round_step_type::Precommit && local_priv_validator_type == RetrySignerClient) {
    timeout = std::chrono::system_clock::duration::zero();
  }
  local_priv_validator_pub_key = local_priv_validator->get_pub_key();
}

void consensus_state::reconstruct_last_commit(state& state_) {
  commit commit_;
  auto ret = block_store_->load_seen_commit(commit_);
  if (!ret || commit_.height != state_.last_block_height) {
    check(block_store_->load_block_commit(state_.last_block_height, commit_),
      fmt::format("failed to reconstruct last commit; commit for height {} not found", state_.last_block_height));
  }

  auto last_precommit = commit_to_vote_set(state_.chain_id, commit_, state_.last_validators);
  check(last_precommit->has_two_thirds_majority(), "failed to reconstruct last commit; does not have +2/3 maj");
  rs.last_commit = std::move(last_precommit);
}

std::shared_ptr<commit> consensus_state::load_commit(int64_t height) {
  std::scoped_lock g(mtx);
  auto ret = std::make_shared<commit>();
  if (height == block_store_->height()) {
    if (block_store_->load_seen_commit(*ret)) {
      // NOTE: Retrieving the height of the most recent block and retrieving
      // the most recent commit does not currently occur as an atomic
      // operation. We check the height and commit here in case a more recent
      // commit has arrived since retrieving the latest height.
      if (ret->height == height) {
        return std::move(ret);
      }
    }
  }
  check(block_store_->load_block_commit(height, *ret), "failed to load commit");
  return std::move(ret);
}

inline fs::path get_wal_file_path(const consensus_config& cs_config) {
  return fs::path(cs_config.root_dir) / fs::path(cs_config.wal_file);
}

bool repair_wal_file(const std::string& src, const std::string& dst) {
  // truncate dst file
  wal_decoder dec{src};
  wal_encoder enc{dst};
  timed_wal_message tmsg{};

  // best-case repair (until first error is encountered)
  auto ret = dec.decode(tmsg);
  while (ret == wal_decoder::result::success) {
    size_t len;
    check(enc.encode(tmsg, len), "failed to encode msg");
    ret = dec.decode(tmsg);
  }
  return true;
}

bool consensus_state::load_wal_file() {
  if (cs_config.wal_file.empty()) {
    cs_config.wal_file = cs_config.wal_path;
  }
  auto wal_file_path = get_wal_file_path(cs_config);
  try {
    if (fs::exists(wal_file_path)) {
      if (!fs::is_directory(wal_file_path)) {
        elog(fmt::format("unable to create wal_file={}", wal_file_path.string()));
        return false;
      }
    } else {
      fs::create_directories(wal_file_path);
    }
    wal_ = std::make_unique<base_wal>(wal_file_path.string(), wal_head_name, wal_file_num, wal_file_size);
  } catch (...) {
    elog("failed to start wal");
    return false;
  }
  return true;
}

void consensus_state::on_start() {
  // We may set the WAL in testing before calling Start, so only OpenWAL if it's still the nilWAL.
  if (dynamic_cast<nil_wal*>(wal_.get())) {
    check(load_wal_file(), "failed to load wal");
  }
  check(wal_->on_start(), "failed to start wal"); // TODO: workaround: directly call on_start instead of start()

  if (do_wal_catchup) {
    bool repair_attempted = false;
    while (true) {
      if (catchup_replay(rs.height)) {
        break;
      }
      // TODO: check this branch, for now cannot be reached
      // if (!data_corruption_error) {
      //   elog("error on catchup replay; proceeding to start state anyway");
      //   break;
      // }
      if (repair_attempted) {
        elog("previous repair attempt failed");
        return; // TODO: return error
      }
      elog("the WAL file is corrupted; attempting repair");

      // 1) prep work
      if (!wal_->on_stop()) { // TODO: change to stop() when WAL supports service method
        return; // TODO: return error
      }
      repair_attempted = true;

      // 2) backup original WAL file
      auto prev_path = get_wal_file_path(cs_config) / wal_head_name;
      auto corrupted_path = prev_path;
      corrupted_path += wal_file_manager::corrupted_postfix;
      try {
        check(!fs::exists(corrupted_path) || fs::is_regular_file(corrupted_path),
          fmt::format("unable to remove previous corrupted wal file={}", corrupted_path.string()));
        fs::rename(prev_path, corrupted_path);
      } catch (...) {
        elog("unable to repair wal file due to unexpected error");
        return; // TODO: return error
      }
      dlog(fmt::format("backed up WAL file src={} dst={}", prev_path.string(), corrupted_path.string()));

      // 3) try to repair (WAL file will be overwritten!)
      if (!repair_wal_file(corrupted_path, prev_path)) {
        elog("the WAL repair failed");
      }

      ilog("successful WAL repair");
      // reload WAL file
      check(load_wal_file(), "failed to load wal");
    }
  }

  enter_new_round(rs.height, rs.round);
}

void consensus_state::on_stop() {
  wal_->on_stop();
  timeout_ticker_timer->cancel();
  thread_pool->stop();
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
  ilog(fmt::format("STH:: sleep for {}", sleep_duration));
  schedule_timeout(std::chrono::microseconds(sleep_duration), rs_.height, 0, round_step_type::NewHeight); // todo
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
    rs.last_commit = std::make_shared<nil_vote_set>();
  } else if (rs.commit_round > -1 && rs.votes != nullptr) {
    // Use rs.votes
    if (!rs.votes->precommits(rs.commit_round)->has_two_thirds_majority()) {
      throw std::runtime_error(fmt::format("wanted to form a commit, but precommits (H/R: {}/{}) didn't have 2/3+",
        state_.last_block_height, rs.commit_round));
    }
    rs.last_commit = rs.votes->precommits(rs.commit_round);
  } else if (rs.last_commit == nullptr) {
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
  update_round_step(0, round_step_type::NewHeight);

  if (rs.commit_time == 0)
    rs.start_time = cs_config.commit(get_time());
  else
    rs.start_time = cs_config.commit(rs.commit_time);

  rs.proposal = {};
  rs.proposal_block = {};
  rs.proposal_block_parts = {};
  rs.locked_round = -1;
  rs.locked_block = {};
  rs.locked_block_parts = {};

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
  auto event = events::event_data_round_state{rs};
  if (!wal_->write({event})) { // TODO: null check for rs or WAL?
    elog("failed writing to WAL");
  }
  n_steps++;

  // newStep is called by updateToState in NewState before the eventBus is set!
  event_bus_->publish_event_new_round_step(event);

  // todo - notify consensus_reactor about rs
  event_switch_mq_channel.publish(appbase::priority::medium,
    std::make_shared<plugin_interface::event_info>(plugin_interface::event_info{EventNewRoundStep, round_state{rs}}));
}

/**
 * receiveRoutine handles messages which may cause state transitions.
 * it's argument (n) is the number of messages to process before exiting - use 0 to run forever
 * It keeps the RoundState and is the only thing that updates it.
 * Updates (state transitions) happen on timeouts, complete proposals, and 2/3 majorities.
 * State must be locked before any internal state is updated.
 */
void consensus_state::receive_routine(p2p::internal_msg_info_ptr mi) {
  message_handler m(shared_from_this());
  if (!wal_->write_sync({*mi})) { // TODO: sync is not needed for receive_message_queue
    elog("failed writing to WAL");
  }
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
  std::scoped_lock g(timeout_ticker_mtx);
  dlog(fmt::format("received tick: old_ti=[duration={} {}/{}/{}], new_ti=[duration={} {}/{}/{}]",
    std::chrono::duration_cast<std::chrono::microseconds>(old_ti->duration_).count(), old_ti->height, old_ti->round,
    round_step_to_str(old_ti->step), std::chrono::duration_cast<std::chrono::microseconds>(ti->duration_).count(),
    ti->height, ti->round, round_step_to_str(old_ti->step)));

  // ignore tickers for old height/round/step
  if (ti->height < old_ti->height) {
    return;
  } else if (ti->height == old_ti->height) {
    if (ti->round < old_ti->round) {
      return;
    } else if (ti->round == old_ti->round) {
      if ((static_cast<int>(old_ti->step) > 0) && (ti->step <= old_ti->step)) {
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
      // wlog("consensus_state timeout error: ${m}", ("m", ec.message()));
    }
    timeout_ticker_channel.publish(appbase::priority::medium, ti); // -> tock : option1 - use channel
    // tock(ti); // -> tock : option2 - directly call
  });
}

void consensus_state::tock(timeout_info_ptr ti) {
  ilog(fmt::format(
    "Timed out: hrs={}/{}/{}, timeout={}", ti->height, ti->round, round_step_to_str(ti->step), ti->duration_.count()));
  if (!wal_->write({*ti})) {
    elog("failed writing to WAL");
  }
  handle_timeout(ti);
}

void consensus_state::handle_timeout(timeout_info_ptr ti) {
  std::scoped_lock g(mtx);
  dlog(fmt::format("Received tock: hrs={}/{}/{}, timeout={}", ti->height, ti->round, round_step_to_str(ti->step),
    ti->duration_.count()));

  // timeouts must be for current height, round, step
  if ((ti->height != rs.height) || (ti->round < rs.round) || (ti->round == rs.round && ti->step < rs.step)) {
    dlog(fmt::format(
      "Ignoring tock because we are ahead: hrs={}/{}/{}", ti->height, ti->round, round_step_to_str(ti->step)));
    return;
  }

  switch (ti->step) {
  case round_step_type::NewHeight:
    enter_new_round(ti->height, 0);
    break;
  case round_step_type::NewRound:
    enter_propose(ti->height, 0);
    break;
  case round_step_type::Propose:
    event_bus_->publish_event_timeout_propose(events::event_data_round_state{rs});
    enter_prevote(ti->height, ti->round);
    break;
  case round_step_type::PrevoteWait:
    event_bus_->publish_event_timeout_wait(events::event_data_round_state{rs});
    enter_precommit(ti->height, ti->round);
    break;
  case round_step_type::PrecommitWait:
    event_bus_->publish_event_timeout_wait(events::event_data_round_state{rs});
    enter_precommit(ti->height, ti->round);
    enter_new_round(ti->height, ti->round + 1);
    break;
  default:
    throw std::runtime_error("invalid timeout step");
  }
}

void consensus_state::enter_new_round(int64_t height, int32_t round) {
  if ((rs.height != height) || (round < rs.round) || (rs.round == round && rs.step != round_step_type::NewHeight)) {
    dlog(fmt::format(
      "entering new round with invalid args: hrs={}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));
    return;
  }

  auto now = get_time();
  if (rs.start_time > now) {
    dlog("need to set a buffer and log message here for sanity");
  }
  dlog(fmt::format("entering new round: current={}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));

  // increment validators if necessary
  auto validators = rs.validators;
  if (rs.round < round) {
    validators->increment_proposer_priority(round - rs.round); // todo - safe sub
  }

  // Setup new round
  // we don't fire newStep for this step,
  // but we fire an event, so update the round step first
  update_round_step(round, round_step_type::NewRound);
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

  // event bus
  event_bus_->publish_event_new_round(events::event_data_new_round{rs});

  // metrics // todo?

  // wait for tx? // todo?
  enter_propose(height, round);
}

bool consensus_state::need_proof_block(int64_t height) {
  if (height == local_state.initial_height) {
    return true;
  }
  block_meta bm;
  check(block_store_->load_block_meta(height - 1, bm),
    fmt::format("needProofBlock: last block meta for height {} not found", height - 1));
  return local_state.app_hash == bm.header.app_hash;
}

void consensus_state::enter_propose(int64_t height, int32_t round) {
  if (rs.height != height || round < rs.round || (rs.round == round && round_step_type::Propose <= rs.step)) {
    dlog(fmt::format(
      "entering propose step with invalid args: {}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));
    return;
  }
  dlog(fmt::format("entering propose step: {}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));

  noir_defer([this, height, round]() {
    update_round_step(round, round_step_type::Propose);
    new_step();
    if (is_proposal_complete())
      enter_prevote(height, rs.round);
  });

  // If we don't get the proposal and all block parts quick enough, enter_prevote
  schedule_timeout(cs_config.propose(round), height, round, round_step_type::Propose);

  // Nothing more to do if we are not a validator
  if (!local_priv_validator) {
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
  if (!rs.proposal || !rs.proposal_block)
    return false;
  // We have the proposal. If there is a POL_round, make sure we have prevotes from it.
  if (rs.proposal->pol_round < 0)
    return true;
  // if this is false the proposer is lying or we haven't received the POL yet
  return rs.votes->prevotes(rs.proposal->pol_round)->has_two_thirds_majority();
}

bool consensus_state::is_proposal(bytes address) {
  return rs.validators->get_proposer()->address == address;
}

void consensus_state::decide_proposal(int64_t height, int32_t round) {
  std::shared_ptr<block> block_;
  std::shared_ptr<part_set> block_parts_;

  // Decide on a block
  if (rs.valid_block) {
    // If there is valid block, choose that
    block_ = rs.valid_block;
    block_parts_ = rs.valid_block_parts;
  } else {
    // Create a new proposal block from state/txs from the mempool
    //    block_ = create_proposal_block();
    if (!local_priv_validator)
      throw std::runtime_error("attempted to create proposal block with empty priv_validator");

    commit commit_;
    std::vector<std::optional<vote>> votes_;
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

    std::tie(block_, block_parts_) =
      block_exec->create_proposal_block(rs.height, local_state, commit_, proposer_addr, votes_);

    if (!block_) {
      wlog("MUST CONNECT TO MEMPOOL IN ORDER TO RETRIEVE SOME BLOCKS"); // todo - remove once mempool is ready
      return;
    }
  }

  // Flush the WAL. Otherwise, we may not recompute the same proposal to sign,
  // and the privValidator will refuse to sign anything.
  if (!wal_->flush_and_sync()) {
    elog("failed flushing WAL to disk");
  }

  // Make proposal
  auto prop_block_id = p2p::block_id{block_->get_hash(), block_parts_->header()};
  auto proposal_ = proposal::new_proposal(height, round, rs.valid_round, prop_block_id);

  if (auto err = local_priv_validator->sign_proposal(proposal_); !err.has_value()) {
    // proposal_.signature = p.signature; // TODO: no need; already updated proposal_.signature

    // Send proposal and block_parts
    internal_mq_channel.publish(
      appbase::priority::medium, std::make_shared<p2p::internal_msg_info>(p2p::internal_msg_info{proposal_, ""}));

    for (auto i = 0; i < block_parts_->total; i++) {
      auto part_ = block_parts_->get_part(i);
      auto msg = p2p::block_part_message{rs.height, rs.round, part_->index, part_->bytes_, part_->proof_};
      internal_mq_channel.publish(
        appbase::priority::medium, std::make_shared<p2p::internal_msg_info>(p2p::internal_msg_info{msg, ""}));
    }
    dlog(fmt::format("signed proposal: height={} round={}", height, round));
  } else {
    elog(fmt::format("propose step; failed signing proposal: height={} round={}", height, round));
  }
}

/**
 * Enter after entering propose (proposal block and POL is ready)
 * Prevote for locked_block if we are locked or proposal_blocke if valid. Otherwise vote nil.
 */
void consensus_state::enter_prevote(int64_t height, int32_t round) {
  if (rs.height != height || round < rs.round || (rs.round == round && round_step_type::Prevote <= rs.step)) {
    dlog(fmt::format(
      "entering prevote step with invalid args: {}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));
    return;
  }
  dlog(fmt::format("entering prevote step: {}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));

  noir_defer([this, height, round]() {
    update_round_step(round, round_step_type::Prevote);
    new_step();
  });

  // Sign and broadcast vote as necessary
  do_prevote(height, round);

  // Once `add_vote` hits any +2/3 prevotes, we will go to prevote_wait
  // (so we have more time to try and collect +2/3 prevotes for a single block)
}

void consensus_state::do_prevote(int64_t height, int32_t round) {
  // If a block is locked, prevote that
  if (rs.locked_block) {
    dlog("prevote step; already locked on a block; prevoting on a locked block");
    sign_add_vote(p2p::signed_msg_type::Prevote, rs.locked_block->get_hash(), rs.locked_block_parts->header());
    return;
  }

  // If proposal_block is nil, prevote nil
  if (!rs.proposal_block) {
    dlog("prevote step; proposal_block is nil");
    sign_add_vote(p2p::Prevote, bytes{} /* todo - nil */, p2p::part_set_header{});
    return;
  }

  // Validate proposal block // todo

  // Prevote rs.proposal_block
  dlog("prevote step; proposal_block is valid");
  sign_add_vote(p2p::Prevote, rs.proposal_block->get_hash(), rs.proposal_block_parts->header());
}

void consensus_state::enter_prevote_wait(int64_t height, int32_t round) {
  if (rs.height != height || round < rs.round || (rs.round == round && round_step_type::PrevoteWait <= rs.step)) {
    dlog(fmt::format(
      "entering prevote_wait step with invalid args: {}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));
    return;
  }

  if (rs.votes->prevotes(round)->has_two_thirds_any()) {
    throw std::runtime_error(
      fmt::format("entering prevote_wait step ({}/{}), but prevotes does not have any 2/3+ votes", height, round));
  }

  dlog(fmt::format("entering prevote_wait step: {}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));

  noir_defer([this, height, round]() {
    update_round_step(round, round_step_type::PrevoteWait);
    new_step();
  });

  // Wait for some more prevotes
  schedule_timeout(cs_config.prevote(round), height, round, round_step_type::PrevoteWait);
}

/**
 * Enter 2/3+ precommits for block or nil.
 * Lock and precommit the proposal_block if we have enough prevotes for it
 * or, unlock an existing lock and precommit nil if 2/3+ of prevotes were nil
 * or, precommit nil
 */
void consensus_state::enter_precommit(int64_t height, int32_t round) {
  if (rs.height != height || round < rs.round || (rs.round == round && round_step_type::Precommit <= rs.step)) {
    dlog(fmt::format(
      "entering precommit step with invalid args: {}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));
    return;
  }
  dlog(fmt::format("entering precommit step: {}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));

  noir_defer([this, height, round]() {
    update_round_step(round, round_step_type::Precommit);
    new_step();
  });

  // Check for a polka
  auto block_id_ = rs.votes->prevotes(round)->two_thirds_majority();
  if (!block_id_.has_value()) {
    // don't have a polka, so precommit nil
    if (rs.locked_block)
      dlog("precommit step; no +2/3 prevotes during enterPrecommit while we are locked; precommitting nil");
    else
      dlog("precommit step; no +2/3 prevotes during enterPrecommit; precommitting nil");
    sign_add_vote(p2p::Precommit, bytes{} /* todo - nil */, p2p::part_set_header{});
    return;
  }

  // At this point 2/3+ prevoted for a block or nil
  // publish event polka // todo - is this necessary?
  event_bus_->publish_event_polka(events::event_data_round_state{rs});

  // latest pol_round should be this round
  auto pol_round = rs.votes->pol_info();
  if (pol_round < round)
    throw std::runtime_error(fmt::format("pol_round should be {} but got {}", round, pol_round));

  // 2/3+ prevoted nil, so unlock and precommit nil
  if (block_id_->hash.empty()) {
    if (!rs.locked_block) {
      dlog("precommit step; +2/3 prevoted for nil");
    } else {
      dlog("precommit step; +2/3 prevoted for nil; unlocking");
      rs.locked_round = -1;
      rs.locked_block = {};
      rs.locked_block_parts = {};
      // publish event unlock
      event_bus_->publish_event_unlock(events::event_data_round_state{rs});
    }
    sign_add_vote(p2p::Precommit, bytes{} /* todo - nil */, p2p::part_set_header{});
    return;
  }

  // At this point, 2/3+ prevoted for a block
  // If we are already locked on the block, precommit it, and update the locked_round
  if (rs.locked_block && rs.locked_block->hashes_to(block_id_->hash)) {
    dlog("precommit step; +2/3 prevoted locked block; relocking");
    rs.locked_round = round;
    // publish event relock
    event_bus_->publish_event_relock(events::event_data_round_state{rs});
    sign_add_vote(p2p::Precommit, block_id_->hash, block_id_->parts);
    return;
  }

  // If +2/3 prevoted for proposal block, stage and precommit it
  if (rs.proposal_block->hashes_to(block_id_->hash)) {
    dlog("precommit step; +2/3 prevoted proposal block; locking hash");

    // Validate block // todo

    rs.locked_round = round;
    rs.locked_block = rs.proposal_block;
    rs.locked_block_parts = rs.proposal_block_parts;

    // publish event lock
    event_bus_->publish_event_lock(events::event_data_round_state{rs});
    sign_add_vote(p2p::Precommit, block_id_->hash, block_id_->parts);
    return;
  }

  // There was a polka in this round for a block we don't have.
  // Fetch that block, unlock, and precommit nil.
  // The +2/3 prevotes for this round is the POL for our unlock.
  dlog("precommit step; +2/3 prevotes for a block we do not have; voting nil");

  rs.locked_round = -1;
  rs.locked_block = {};
  rs.locked_block_parts = {};

  if (!rs.proposal_block_parts->has_header(block_id_->parts)) {
    rs.proposal_block = {};
    rs.proposal_block_parts = part_set::new_part_set_from_header(block_id_->parts);
  }

  // publish event unlock
  event_bus_->publish_event_unlock(events::event_data_round_state{rs});
  sign_add_vote(p2p::Precommit, bytes{} /* todo - nil */, p2p::part_set_header{});
}

/**
 * Enter any 2/3+ precommits for next round
 */
void consensus_state::enter_precommit_wait(int64_t height, int32_t round) {
  if (rs.height != height || round < rs.round || (rs.round == round && rs.triggered_timeout_precommit)) {
    dlog(fmt::format("entering precommit_wait step with invalid args: {}/{} triggered_timeout={}", rs.height, rs.round,
      rs.triggered_timeout_precommit));
    return;
  }

  if (!rs.votes->precommits(round)->has_two_thirds_any()) {
    throw std::runtime_error(
      fmt::format("entering precommit_wait step ({}/{}), but prevotes does not have any 2/3+ votes", height, round));
  }

  dlog(fmt::format("entering precommit_wait step: {}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));

  noir_defer([this, height, round]() {
    rs.triggered_timeout_precommit = true;
    new_step();
  });

  // wait for more precommits
  schedule_timeout(cs_config.precommit(round), height, round, round_step_type::PrecommitWait);
}

void consensus_state::enter_commit(int64_t height, int32_t round) {
  if (rs.height != height || round_step_type::Commit <= rs.step) {
    dlog(
      fmt::format("entering commit step with invalid args: {}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));
    return;
  }
  dlog(fmt::format("entering commit step: {}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));

  noir_defer([this, height, round]() {
    update_round_step(round, round_step_type::Commit);
    rs.commit_round = round;
    rs.commit_time = get_time();
    new_step();

    // Maybe finalize immediately
    try_finalize_commit(height);
  });

  auto block_id_ = rs.votes->precommits(round)->two_thirds_majority();
  if (!block_id_.has_value())
    throw std::runtime_error("RunActionCommit() expects +2/3 precommits");

  // The Locked* fields no longer matter.
  // Move them over to ProposalBlock if they match the commit hash,
  // otherwise they'll be cleared in update_to_state.
  if (rs.locked_block->hashes_to(block_id_->hash)) {
    dlog("commit is for a locked block; set ProposalBlock=LockedBlock");
    rs.proposal_block = rs.locked_block;
    rs.proposal_block_parts = rs.locked_block_parts;
  }

  // If we don't have the block being committed, set up to get it.
  if (!rs.proposal_block->hashes_to(block_id_->hash)) {
    if (!rs.proposal_block_parts->has_header(block_id_->parts)) {
      ilog("commit is for a block we do not know about; set ProposalBlock=nil");
      // We're getting the wrong block.
      // Set up ProposalBlockParts and keep waiting.
      rs.proposal_block = {};
      rs.proposal_block_parts = part_set::new_part_set_from_header(block_id_->parts);

      event_bus_->publish_event_valid_block(events::event_data_round_state{rs});
      event_switch_mq_channel.publish(appbase::priority::medium,
        std::make_shared<plugin_interface::event_info>(plugin_interface::event_info{EventValidBlock, round_state{rs}}));
    }
  }
}

void consensus_state::try_finalize_commit(int64_t height) {
  if (rs.height != height)
    throw std::runtime_error(fmt::format("try_finalize_commit: rs.height={} vs height={}", rs.height, height));

  auto block_id_ = rs.votes->precommits(rs.commit_round)->two_thirds_majority();
  if (!block_id_.has_value() || block_id_->hash.empty()) {
    elog("failed attempt to finalize commit; there was no +2/3 majority or +2/3 was for nil");
    return;
  }

  if (!rs.proposal_block->hashes_to(block_id_->hash)) {
    dlog("failed attempt to finalize commit; we do not have the commit block");
    return;
  }
  finalize_commit(height);
}

void consensus_state::finalize_commit(int64_t height) {
  if (rs.height != height || rs.step != round_step_type::Commit) {
    dlog(fmt::format(
      "entering finalize commit step with invalid args: {}/{}/{}", rs.height, rs.round, round_step_to_str(rs.step)));
    return;
  }

  auto block_id_ = rs.votes->precommits(rs.commit_round)->two_thirds_majority();
  auto block_ = rs.proposal_block;
  auto block_parts_ = rs.proposal_block_parts;

  if (!block_id_.has_value())
    throw std::runtime_error("panic: cannot finalize commit; commit does not have 2/3 majority");
  if (!block_parts_->has_header(block_id_->parts))
    throw std::runtime_error("panic: expected ProposalBlockParts header to be commit header");
  if (!block_->hashes_to(block_id_->hash))
    throw std::runtime_error("panic: cannot finalize commit; proposal block does not hash to commit hash");

  // Validate block
  if (!block_exec->validate_block(local_state, block_)) {
    throw std::runtime_error("panic: +2/3 committed an invalid block");
  }

  ilog(fmt::format("finalizing commit of block: hash={}", to_hex(block_id_->hash)));
  dlog(fmt::format("block: hash={}", to_hex(block_->get_hash())));

  // Save to block_store
  if (block_store_->height() < block_->header.height) {
    auto precommits = rs.votes->precommits(rs.commit_round);
    auto seen_commit = precommits->make_commit();
    block_store_->save_block(*block_, *block_parts_, seen_commit);
  } else {
    dlog("calling finalizeCommit on already stored block");
  }

  // Write EndHeightMessage{} for this height, implying that the blockstore has saved the block.
  if (!wal_->write_sync({end_height_message{height}})) {
    throw std::runtime_error(fmt::format(
      "failed to write end_height_message at height:{} to consensus WAL; check your file system and restart the node",
      height));
  }

  auto state_copy = local_state;

  // Apply block // todo - make it work; some operations are commented out now
  auto result = block_exec->apply_block(state_copy, p2p::block_id{block_->get_hash(), block_parts_->header()}, block_);
  if (!result.has_value()) {
    elog("failed to apply block");
    return;
  }
  state_copy = result.value();

  // record metric

  // New Height Step!
  update_to_state(state_copy);

  // Private validator might have changed it's key pair => refetch pubkey.
  update_priv_validator_pub_key();

  // cs.StartTime is already set.
  // Schedule Round0 to start soon.
  schedule_round_0(rs);

  // By here,
  // * cs.Height has been increment to height+1
  // * cs.Step is now cstypes.RoundStepNewHeight
  // * cs.StartTime is set to when we will start round0.
}

void consensus_state::set_proposal(p2p::proposal_message& msg) {
  if (rs.proposal) {
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

  // Verify signature
  auto proposal_sign_bytes = encode(canonical::canonicalize_proposal(msg));
  if (!rs.validators->get_proposer()->pub_key_.verify_signature(proposal_sign_bytes, msg.signature)) {
    dlog("set_proposal; error invalid proposal signature");
    return;
  }

  rs.proposal = std::make_shared<p2p::proposal_message>(p2p::proposal_message{msg});
  // We don't update cs.ProposalBlockParts if it is already set.
  // This happens if we're already in cstypes.RoundStepCommit or if there is a valid block in the current round.
  if (!rs.proposal_block_parts) {
    rs.proposal_block_parts = part_set::new_part_set_from_header(msg.block_id_.parts);
  }

  ilog(fmt::format("received proposal; {}", msg.type));
}

/**
 * Asynchronously triggers either enterPrevote (before we timeout of propose) or tryFinalizeCommit, once we have full
 * block NOTE: block may be invalid
 */
bool consensus_state::add_proposal_block_part(p2p::block_part_message& msg, node_id peer_id) {
  auto height_ = msg.height;
  auto round_ = msg.round;
  auto part_ = std::make_shared<part>(part{msg.index, msg.bytes_, msg.proof});

  // Blocks might be reused, so round mismatch is OK
  if (rs.height != height_) {
    dlog(fmt::format("received block_part from wrong height: height={} round={}", height_, round_));
    return false;
  }

  // We are not expecting a block part
  if (!rs.proposal_block_parts) {
    // NOTE: this can happen when we've gone to a higher round and
    // then receive parts from the previous round - not necessarily a bad peer.
    dlog(fmt::format("received block_part when we are not expecting any: height={} round={}", height_, round_));
    return false;
  }

  auto added = rs.proposal_block_parts->add_part(part_);

  if (rs.proposal_block_parts->byte_size > local_state.consensus_params_.block.max_bytes) {
    elog(fmt::format("total size of proposal block parts exceeds maximum block bytes ({} > {})",
      rs.proposal_block_parts->byte_size, local_state.consensus_params_.block.max_bytes));
    return added;
  }
  if (added && rs.proposal_block_parts->is_complete()) {
    auto new_block_ = block::new_block_from_part_set(rs.proposal_block_parts); // todo - check if this is correct
    if (!new_block_) {
      elog("unable to convert from part_set");
      return added;
    }
    rs.proposal_block = new_block_;

    // NOTE: it's possible to receive complete proposal blocks for future rounds without having the proposal
    ilog(fmt::format("received complete proposal block: height={}", rs.proposal_block->header.height));

    event_bus_->publish_event_complete_proposal(events::event_data_complete_proposal{rs});

    // Update Valid if we can
    auto prevotes = rs.votes->prevotes(rs.round);
    auto block_id_ = prevotes->two_thirds_majority();
    if (block_id_.has_value() && !block_id_->is_zero() && rs.valid_round < rs.round) {
      if (rs.proposal_block->hashes_to(block_id_->hash)) {
        dlog("updating valid block to new proposal block");
        rs.valid_round = rs.round;
        rs.valid_block = rs.proposal_block;
        rs.valid_block_parts = rs.proposal_block_parts;
      }
    }

    if (rs.step <= round_step_type::Propose && is_proposal_complete()) {
      // Move to the next step
      enter_prevote(height_, rs.round);
      if (block_id_.has_value())
        enter_precommit(height_, rs.round);
    } else if (rs.step == round_step_type::Commit) {
      // If we're waiting on the proposal block...
      try_finalize_commit(height_);
    }
    return added;
  }
  return added;
}

/**
 * Attempt to add vote. If it's a duplicate signature, dupeout? the validator
 */
bool consensus_state::try_add_vote(p2p::vote_message& msg, node_id peer_id) {
  auto vote_ = vote{msg};
  auto added = add_vote(vote_, peer_id);

  // todo - implement; it's very long
  // todo - must handle err from add_vote

  return added;
}

bool consensus_state::add_vote(vote& vote_, node_id peer_id) {
  dlog(fmt::format("adding vote: height={} type={} index={} cs_height={}", vote_.height, vote_.type,
    vote_.validator_index, rs.height));

  // A precommit for the previous height?
  // These come in while we wait timeoutCommit
  if (vote_.height + 1 == rs.height && vote_.type == p2p::Precommit) {
    if (rs.step != round_step_type::NewHeight) {
      dlog("precommit vote came in after commit timeout and has been ignored");
      return false;
    }
    auto added = rs.last_commit->add_vote(vote_);
    if (!added)
      return false;

    dlog("added vote to last precommits");
    event_bus_->publish_event_vote(events::event_data_vote{.vote = vote_});

    event_switch_mq_channel.publish(appbase::priority::medium,
      std::make_shared<plugin_interface::event_info>(
        plugin_interface::event_info{EventVote, p2p::vote_message{vote_}}));

    // if we can skip timeoutCommit and have all the votes now,
    if (cs_config.skip_timeout_commit && rs.last_commit->has_all()) {
      // go straight to new round (skip timeout commit)
      // cs.scheduleTimeout(time.Duration(0), cs.Height, 0, cstypes.RoundStepNewHeight)
      enter_new_round(rs.height, 0);
    }
    return false;
  }

  // Height mismatch is ignored.
  // Not necessarily a bad peer, but not favorable behavior.
  if (vote_.height != rs.height) {
    dlog(fmt::format(
      "vote ignored and not added: vote_height={} cs_height={} peer_id={}", vote_.height, rs.height, peer_id));
    return false;
  }

  // Verify VoteExtension if precommit
  if (vote_.type == p2p::Precommit) {
    auto err = block_exec->verify_vote_extension(vote_);
    if (err.has_value()) {
      elog(fmt::format("{}", err.value()));
      return false;
    }
  }

  auto height = rs.height;
  auto added = rs.votes->add_vote(vote_, peer_id);
  if (!added) {
    // Either duplicate, or error upon cs.Votes.AddByIndex()
    return false;
  }

  event_bus_->publish_event_vote(events::event_data_vote{.vote = vote_});
  event_switch_mq_channel.publish(appbase::priority::medium,
    std::make_shared<plugin_interface::event_info>(plugin_interface::event_info{EventVote, p2p::vote_message{vote_}}));

  switch (vote_.type) {
  case p2p::Prevote: {
    auto prevotes = rs.votes->prevotes(vote_.round);
    dlog("added vote to prevote");
    auto block_id_ = prevotes->two_thirds_majority();

    // If +2/3 prevotes for a block or nil for *any* round:
    if (block_id_.has_value()) {
      // There was a polka!
      // If we're locked but this is a recent polka, unlock.
      // If it matches our ProposalBlock, update the ValidBlock

      // Unlock if `cs.LockedRound < vote.Round <= cs.Round`
      // NOTE: If vote.Round > cs.Round, we'll deal with it when we get to vote.Round
      if (rs.locked_block && rs.locked_round < vote_.round && vote_.round <= rs.round &&
        !rs.locked_block->hashes_to(block_id_->hash)) {
        dlog(fmt::format("unlocking because of POL: locked_round={} pol_round={}", rs.locked_round, vote_.round));
        rs.locked_round = -1;
        rs.locked_block = {};
        rs.locked_block_parts = {};

        event_bus_->publish_event_unlock(events::event_data_round_state{rs});
      }

      // Update Valid* if we can.
      // NOTE: our proposal block may be nil or not what received a polka
      if (!block_id_->hash.empty() && rs.valid_round < vote_.round && vote_.round == rs.round) {
        if (rs.proposal_block->hashes_to(block_id_->hash)) {
          dlog(fmt::format(
            "updating valid block because of POL: valid_round={} pol_round={}", rs.valid_round, vote_.round));
          rs.valid_round = vote_.round;
          rs.valid_block = rs.proposal_block;
          rs.valid_block_parts = rs.proposal_block_parts;
        } else {
          dlog("valid block we do not know about; set ProposalBlock=nil");
          // we're getting the wrong block
          rs.proposal_block = {};
        }

        if (!rs.proposal_block_parts->has_header(block_id_->parts)) {
          rs.proposal_block_parts = part_set::new_part_set_from_header(block_id_->parts);
        }

        event_switch_mq_channel.publish(appbase::priority::medium,
          std::make_shared<plugin_interface::event_info>(
            plugin_interface::event_info{EventValidBlock, round_state{rs}}));

        event_bus_->publish_event_valid_block(events::event_data_round_state{rs});
      }
    }

    // If +2/3 prevotes for *anything* for future round:
    if (rs.round < vote_.round && prevotes->has_two_thirds_any()) {
      // Round-skip if there is any 2/3+ of votes ahead of us
      enter_new_round(height, vote_.round);
    } else if (rs.round == vote_.round && round_step_type::Prevote <= rs.step) {
      auto b_id = prevotes->two_thirds_majority();
      if (b_id.has_value() && (is_proposal_complete() || b_id->hash.empty()))
        enter_precommit(height, vote_.round);
      else if (prevotes->has_two_thirds_any())
        enter_prevote_wait(height, vote_.round);
    } else if (rs.proposal && 0 <= rs.proposal->pol_round && rs.proposal->pol_round == vote_.round) {
      // If the proposal is now complete, enter prevote of cs.Round.
      if (is_proposal_complete())
        enter_prevote(height, rs.round);
    }
    break;
  }
  case p2p::Precommit: {
    auto precommits = rs.votes->precommits(vote_.round);
    dlog("added vote to precommit");
    auto block_id_ = precommits->two_thirds_majority();
    if (block_id_.has_value()) {
      // Executed as TwoThirdsMajority could be from a higher round
      enter_new_round(height, vote_.round);
      enter_precommit(height, vote_.round);

      if (!block_id_->hash.empty()) {
        enter_commit(height, vote_.round);
        if (cs_config.skip_timeout_commit && precommits->has_all())
          enter_new_round(rs.height, 0);
      } else {
        enter_precommit_wait(height, vote_.round);
      }
    } else if (rs.round <= vote_.round && precommits->has_two_thirds_any()) {
      enter_new_round(height, vote_.round);
      enter_precommit_wait(height, vote_.round);
    }
    break;
  }
  default:
    throw std::runtime_error(fmt::format("unexpected vote type={}", vote_.type));
  }

  return added;
}

std::optional<vote> consensus_state::sign_vote(p2p::signed_msg_type msg_type, bytes hash, p2p::part_set_header header) {
  // Flush the WAL. Otherwise, we may not recompute the same vote to sign,
  // and the privValidator will refuse to sign anything.
  if (!wal_->flush_and_sync()) {
    elog("failed to flush wal");
    return {};
  }

  if (local_priv_validator_pub_key.empty()) {
    elog("pubkey is not set. Look for \"Can't get private validator pubkey\" errors");
    return {};
  }

  auto addr = local_priv_validator_pub_key.address();
  auto val_idx = rs.validators->get_index_by_address(addr);
  if (val_idx < 0) {
    elog("sign_vote failed: unable to determine validator index");
    return {};
  }

  auto vote_ = vote{msg_type, rs.height, rs.round, p2p::block_id{hash, header}, vote_time(), addr, val_idx};

  // If the signedMessageType is for precommit,
  // use our local precommit Timeout as the max wait time for getting a singed commit. The same goes for prevote.
  std::chrono::system_clock::duration timeout;

  switch (msg_type) {
  case p2p::Precommit: {
    timeout = cs_config.timeout_precommit;
    // if the signedMessage type is for a precommit, add VoteExtension
    auto ext = block_exec->extend_vote(vote_);
    vote_.vote_extension_ = ext;
    break;
  }
  case p2p::Prevote:
    timeout = cs_config.timeout_prevote;
  default:
    timeout = std::chrono::seconds(1);
  }

  local_priv_validator->sign_vote(vote_);
  // vote_.signature = v.signature; // TODO: remove; no need as vote_.signature is already updated
  // vote_.timestamp = v.timestamp; // TODO: need to implement inside sign_vote
  return vote_;
}

/**
 * Ensure monotonicity of the time a validator votes on.
 * It ensures that for a prior block with a BFT-timestamp of T,
 * any vote from this validator will have time at least time T + 1ms.
 * This is needed, as monotonicity of time is a guarantee that BFT time provides.
 */
p2p::tstamp consensus_state::vote_time() {
  auto now = get_time();
  auto min_vote_time = now;
  // Minimum time increment between blocks
  if (rs.locked_block) {
    min_vote_time = rs.locked_block->header.time + std::chrono::milliseconds(1).count();
  } else if (rs.proposal_block) {
    min_vote_time = rs.proposal_block->header.time + std::chrono::milliseconds(1).count();
  }

  if (now > min_vote_time)
    return now;
  return min_vote_time;
}

/**
 * sign vote and publish on internal_msg_channel
 */
vote consensus_state::sign_add_vote(p2p::signed_msg_type msg_type, bytes hash, p2p::part_set_header header) {
  if (!local_priv_validator)
    return {};

  if (local_priv_validator_pub_key.empty()) {
    // Vote won't be signed, but it's not critical
    elog("sign_add_vote: pubkey is not set. Look for \"Can't get private validator pubkey\" errors");
    return {};
  }

  // If the node not in the validator set, do nothing.
  if (!rs.validators->has_address(local_priv_validator_pub_key.address()))
    return {};

  auto vote_ = sign_vote(msg_type, hash, header);
  if (vote_.has_value()) {
    internal_mq_channel.publish(appbase::priority::medium,
      std::make_shared<p2p::internal_msg_info>(p2p::internal_msg_info{p2p::vote_message(vote_.value()), ""}));
    dlog(fmt::format("signed and pushed vote: height={} round={}", rs.height, rs.round));
    return vote_.value();
  }

  dlog(fmt::format("failed signing vote: height={} round={}", rs.height, rs.round));
  return {};
}

} // namespace noir::consensus
