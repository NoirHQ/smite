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
  std::map<std::string, std::shared_ptr<peer_state>> peers;
  bool wait_sync;

  // Receive events from consensus_state
  plugin_interface::egress::channels::event_switch_message_queue::channel_type::handle event_switch_mq_subscription =
    appbase::app().get_channel<plugin_interface::egress::channels::event_switch_message_queue>().subscribe(
      std::bind(&consensus_reactor::process_event, this, std::placeholders::_1));

  // Received network messages from peers
  plugin_interface::incoming::channels::peer_message_queue::channel_type::handle peer_mq_subscription =
    appbase::app().get_channel<plugin_interface::incoming::channels::peer_message_queue>().subscribe(
      std::bind(&consensus_reactor::process_peer_msg, this, std::placeholders::_1));

  consensus_reactor(std::shared_ptr<consensus_state> new_cs_state, bool new_wait_sync)
    : cs_state(std::move(new_cs_state)), wait_sync(new_wait_sync) {}

  static std::shared_ptr<consensus_reactor> new_consensus_reactor(
    std::shared_ptr<consensus_state>& new_cs_state, bool new_wait_sync) {
    auto consensus_reactor_ = std::make_shared<consensus_reactor>(new_cs_state, new_wait_sync);
    return consensus_reactor_;
  }

  void on_start() {
    cs_state->on_start();
  }

  void process_event(const plugin_interface::event_info_ptr& info) {
    switch (info->event_) {
    case EventNewRoundStep:
      broadcast_new_round_step_message(std::get<round_state>(info->message_));
      break;
    case EventValidBlock:
      broadcast_new_valid_block_message(std::get<round_state>(info->message_));
      break;
    case EventVote:
      broadcast_has_vote_message(std::get<p2p::vote_message>(info->message_));
      break;
    }
  }

  void on_stop() {}

  std::shared_ptr<peer_state> get_peer_state(std::string peer_id) {
    std::lock_guard<std::mutex> g(mtx);
    auto it = peers.find(peer_id);
    return it == peers.end() ? nullptr : it->second;
  }

  void broadcast_new_round_step_message(const round_state& rs) {
    auto msg = make_round_step_message(rs);
    // TODO: broadcast
  }

  void broadcast_new_valid_block_message(const round_state& rs) {
    auto msg = p2p::new_valid_block_message{rs.height, rs.round,
      rs.proposal_block_parts->header() /* TODO: check if this is correct*/, rs.proposal_block_parts->get_bit_array(),
      rs.step == p2p::RoundStepCommit};
    // TODO: broadcast
  }

  void broadcast_has_vote_message(const p2p::vote_message& vote_) {
    auto msg = p2p::has_vote_message{vote_.height, vote_.round, vote_.type, vote_.validator_index};
    // TODO: broadcast
  }

  p2p::new_round_step_message make_round_step_message(const round_state& rs) {
    return p2p::new_round_step_message{
      rs.height, rs.round, rs.step, rs.start_time /* TODO: find elapsed seconds*/, rs.last_commit->round};
  }

  void sendNewRoundStepMessage() {}

  void gossipVotesForHeight() {}

  void handleStateMessage() {}

  void handleVoteMessage() {}

  void handleMessage() {}

  template<class... Ts>
  struct overload : Ts... {
    using Ts::operator()...;
  };
  template<class... Ts>
  overload(Ts...) -> overload<Ts...>;

  void process_peer_msg(p2p::p2p_msg_info_ptr info) {
    if (info->broadcast) {
      /* TODO: how to handle broadcast?
       * need to check if msg is known (look up in a cache)
       * if already seen one, discard
       */
    }
    auto from = info->peer_id;
    dlog(fmt::format("received message={} from {}", info->msg.index(), from));
    auto ps = get_peer_state(from);
    if (!ps) {
      wlog(fmt::format("unable to find peer_state for from={}", from));
      return;
    }

    std::visit(overload{///
                 /***************************************************************************************************/
                 ///< peer update messages
                 [](p2p::handshake_message& msg) {}, [](p2p::go_away_message& msg) {}, [](p2p::time_message& msg) {},
                 // TODO: how to handle peer update?
                 // TODO: peer up
                 // TODO: peer down
                 /***************************************************************************************************/
                 ///< state messages: new_round_step, new_valid_block, has_vote, vote_set_maj23
                 [this, &ps](p2p::new_round_step_message& msg) {
                   std::unique_lock<std::mutex> lock(cs_state->mtx);
                   auto initial_height = cs_state->local_state.initial_height;
                   lock.unlock();
                   // msg.validate() // TODO
                   ps->apply_new_round_step_message(msg);
                 },
                 [&ps](p2p::new_valid_block_message& msg) { ps->apply_new_valid_block_message(msg); },
                 [&ps](p2p::has_vote_message& msg) { ps->apply_has_vote_message(msg); },
                 [this, &ps](p2p::vote_set_maj23_message& msg) {
                   std::unique_lock<std::mutex> lock(cs_state->mtx);
                   auto height = cs_state->rs.height;
                   auto votes = cs_state->rs.votes;
                   lock.unlock();
                   if (height != msg.height)
                     return;

                   // Peer claims to have a maj23 for some block_id
                   if (auto err = votes->set_peer_maj23(msg.round, msg.type, ps->peer_id, msg.block_id_);
                       err.has_value()) {
                     elog("${err}", ("err", err));
                     return;
                   }

                   // Respond with a vote_set_bits_message, include votes we have and don't have
                   std::shared_ptr<bit_array> our_votes{};
                   switch (msg.type) {
                   case p2p::Prevote:
                     our_votes = votes->prevotes(msg.round)->bit_array_by_block_id(msg.block_id_);
                     break;
                   case p2p::Precommit:
                     our_votes = votes->precommits(msg.round)->bit_array_by_block_id(msg.block_id_);
                     break;
                   default:
                     check(false, "bad VoteSetBitsMessage field type; forgot to add a check in ValidateBasic?");
                   }

                   // TODO: send back vote_set_bits
                 },
                 /***************************************************************************************************/
                 ///< data messages: proposal, proposal_pol, block_part
                 [&ps](p2p::proposal_message& msg) {
                   ps->set_has_proposal(msg);
                   // TODO: r.state.peerMsgQueue <- msgInfo{pMsg, envelope.From}
                 },
                 [&ps](p2p::proposal_pol_message& msg) { ps->apply_proposal_pol_message(msg); },
                 [&ps](p2p::block_part_message& msg) {
                   ps->set_has_proposal_block_part(msg.height, msg.round, msg.index);
                   // TODO: r.state.peerMsgQueue <- msgInfo{bpMsg, envelope.From}
                 },
                 /***************************************************************************************************/
                 ///< vote message: vote
                 [this, &ps](p2p::vote_message& msg) {
                   std::unique_lock<std::mutex> lock(cs_state->mtx);
                   auto height = cs_state->rs.height;
                   auto val_size = cs_state->rs.validators->size();
                   auto last_commit_size = cs_state->rs.last_commit->get_size();
                   lock.unlock();

                   ps->ensure_vote_bit_arrays(height, val_size);
                   ps->ensure_vote_bit_arrays(height - 1, last_commit_size);
                   ps->set_has_vote(msg);
                   // TODO: r.state.peerMsgQueue <- msgInfo{vMsg, envelope.From}
                 },
                 /***************************************************************************************************/
                 ///< vote_set_bits message: vote_set_bits
                 [this, &ps](p2p::vote_set_bits_message& msg) {
                   std::unique_lock<std::mutex> lock(cs_state->mtx);
                   auto height = cs_state->rs.height;
                   auto votes = cs_state->rs.votes;
                   lock.unlock();

                   if (height == msg.height) {
                     std::shared_ptr<bit_array> our_votes{};
                     switch (msg.type) {
                     case p2p::Prevote:
                       our_votes = votes->prevotes(msg.round)->bit_array_by_block_id(msg.block_id_);
                       break;
                     case p2p::Precommit:
                       our_votes = votes->precommits(msg.round)->bit_array_by_block_id(msg.block_id_);
                       break;
                     default:
                       check(false, "bad VoteSetBitsMessage field type; forgot to add a check in ValidateBasic?");
                     }
                     ps->apply_vote_set_bits_message(msg, our_votes);
                   } else {
                     ps->apply_vote_set_bits_message(msg, nullptr);
                   }
                 }},
      info->msg);
  }
};

} // namespace noir::consensus
