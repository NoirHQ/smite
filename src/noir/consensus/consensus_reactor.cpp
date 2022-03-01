// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/consensus_reactor.h>

#include <noir/core/types.h>

namespace noir::consensus {

template<class... Ts>
struct overload : Ts... {
  using Ts::operator()...;
};
template<class... Ts>
overload(Ts...) -> overload<Ts...>;

void consensus_reactor::process_peer_update(const std::string& peer_id, p2p::peer_status status) {
  dlog(fmt::format("peer update: peer_id={}, status={}", peer_id, p2p::peer_status_to_str(status)));
  std::lock_guard<std::mutex> g(mtx);
  switch (status) {
  case p2p::peer_status::up: {
    auto it = peers.find(peer_id);
    if (it == peers.end()) {
      auto ps = peer_state::new_peer_state(peer_id);
      peers[peer_id] = ps;
      it = peers.find(peer_id);
    }
    if (!it->second->is_running) {
      it->second->is_running = true;

      // Start threads for this peer
      // gossip(it->second) // TODO

      if (!wait_sync) {
        // send_new_round_step_message(peer_id); // TODO
      }
    }
    break;
  }
  case p2p::peer_status::down: {
    auto it = peers.find(peer_id);
    if (it != peers.end() && it->second->is_running) {
      // TODO: stop all threads

      peers.erase(it);
    }
    break;
  }
  default:
    break;
  }
}

void consensus_reactor::process_peer_msg(p2p::envelope_ptr info) {
  if (info->broadcast) {
    /* TODO: how to handle broadcast?
     * need to check if msg is known (look up in a cache)
     * if already seen one, discard
     */
  }
  auto from = info->from;

  noir::core::codec::datastream<char> ds(info->message.data(), info->message.size());
  p2p::reactor_message msg;
  ds >> msg;

  dlog(fmt::format("received message={} from='{}'", msg.index(), from));
  auto ps = get_peer_state(from);
  if (!ps) {
    wlog(fmt::format("unable to find peer_state for from='{}'", from));
    return;
  }

  std::visit(overload{///
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
               [this, &ps, &from](p2p::vote_set_maj23_message& msg) {
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

                 auto response = p2p::vote_set_bits_message{msg.height, msg.round, msg.type, msg.block_id_, our_votes};
                 transmit_new_envelope("", from, response);
               },
               /***************************************************************************************************/
               ///< data messages: proposal, proposal_pol, block_part
               [this, &ps, &from](p2p::proposal_message& msg) {
                 ps->set_has_proposal(msg);
                 transmit_new_envelope("", from, msg);
               },
               [&ps](p2p::proposal_pol_message& msg) { ps->apply_proposal_pol_message(msg); },
               [this, &ps, &from](p2p::block_part_message& msg) {
                 ps->set_has_proposal_block_part(msg.height, msg.round, msg.index);
                 transmit_new_envelope("", from, msg);
               },
               /***************************************************************************************************/
               ///< vote message: vote
               [this, &ps, &from](p2p::vote_message& msg) {
                 std::unique_lock<std::mutex> lock(cs_state->mtx);
                 auto height = cs_state->rs.height;
                 auto val_size = cs_state->rs.validators->size();
                 auto last_commit_size = cs_state->rs.last_commit->get_size();
                 lock.unlock();

                 ps->ensure_vote_bit_arrays(height, val_size);
                 ps->ensure_vote_bit_arrays(height - 1, last_commit_size);
                 ps->set_has_vote(msg);

                 transmit_new_envelope("", from, msg);
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
    msg);
}

void consensus_reactor::transmit_new_envelope(
  std::string from, std::string to, const p2p::reactor_message& msg, bool broadcast, int priority) {
  dlog(fmt::format("transmitting a new envelope: to={}, msg_type={}, broadcast={}", to, msg.index(), broadcast));
  auto new_env = std::make_shared<p2p::envelope>();
  new_env->from = from;
  new_env->to = to;
  new_env->broadcast = broadcast;

  const uint32_t payload_size = noir::core::codec::encode_size(msg);
  new_env->message.resize(payload_size);
  noir::core::codec::datastream<char> ds(new_env->message.data(), payload_size);
  ds << msg;

  xmt_mq_channel.publish(priority, new_env);
}

} // namespace noir::consensus
