// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/overloaded.h>
#include <noir/consensus/consensus_reactor.h>

#include <noir/core/types.h>

namespace noir::consensus {

void consensus_reactor::process_peer_update(const std::string& peer_id, p2p::peer_status status) {
  dlog(fmt::format("peer update: peer_id={}, status={}", peer_id, p2p::peer_status_to_str(status)));
  std::lock_guard<std::mutex> g(mtx);
  switch (status) {
  case p2p::peer_status::up: {
    auto it = peers.find(peer_id);
    if (it == peers.end()) {
      auto ps = peer_state::new_peer_state(peer_id, thread_pool->get_executor());
      peers[peer_id] = ps;
      it = peers.find(peer_id);
    }
    if (!it->second->is_running) {
      it->second->is_running = true;

      // Start gossips for this peer
      // gossip_data_routine(it->second); // TODO: uncomment
      // gossip_votes_routine(it->second); // TODO: uncomment
      // query_maj23_routine(it->second); // TODO: uncomment

      // if (!wait_sync)
      //  send_new_round_step_message(peer_id); // TODO: uncomment
    }
    break;
  }
  case p2p::peer_status::down: {
    auto it = peers.find(peer_id);
    if (it != peers.end() && it->second->is_running) {
      it->second->is_running = false; // TODO: is this enough to stop all threads?
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

  std::visit(overloaded{///
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

void consensus_reactor::gossip_data_routine(std::shared_ptr<peer_state> ps) {
  // Check if cs_reactor is running // TODO
  ps->strand->post([this, ps{std::move(ps)}]() {
    while (true) {
      if (!ps->is_running)
        return;

      auto rs = cs_state->get_round_state();
      auto prs = ps->get_round_state();

      // Send proposal_block_parts
      if (rs->proposal_block_parts->has_header(prs->proposal_block_part_set_header)) {
        if (auto [index, ok] =
              rs->proposal_block_parts->get_bit_array()->sub(prs->proposal_block_parts->copy())->pick_random();
            ok) {
          auto part = rs->proposal_block_parts->get_part(index);
          dlog(fmt::format("sending block_part: height={} round={}", prs->height, prs->round));
          transmit_new_envelope(
            "", ps->peer_id, p2p::block_part_message{rs->height, rs->round, part->index, part->bytes_, part->proof_});
          ps->set_has_proposal_block_part(prs->height, prs->round, index);
          continue;
        }
      }

      // If peer is on a previous height, help catching up
      auto block_store_base = cs_state->block_store_->base();
      if (block_store_base > 0 && 0 < prs->height && prs->height < rs->height && prs->height >= block_store_base) {
        // If we never received commit_message from peer, block_parts will not be initialized
        if (prs->proposal_block_parts == nullptr) {
          block_meta block_meta_;
          if (!cs_state->block_store_->load_block_meta(prs->height, block_meta_)) {
            elog("failed to load block_meta");
            std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration); // TODO: check
          } else {
            ps->init_proposal_block_parts(block_meta_.bl_id.parts);
          }
          continue;
        }
        gossip_data_for_catchup(rs, prs, ps);
        continue;
      }

      // If height and round don't match
      if (rs->height != prs->height || rs->round != prs->round) {
        std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration); // TODO: check
        continue;
      }

      /* By here, height and round should match.
       * Proposal block_parts were matched and sent if any were needed.
       */

      // Send proposal
      if (rs->proposal != nullptr && !prs->proposal) {
        // Proposal: share meta_data with peer
        {
          transmit_new_envelope("", ps->peer_id, p2p::proposal_message{*rs->proposal});
          ps->set_has_proposal(*rs->proposal);
        }

        // Proposal_pol: allows peer to know which pol_votes we have. The peer must receive proposal first
        if (0 <= rs->proposal->pol_round) {
          auto p_pol = rs->votes->prevotes(rs->proposal->pol_round)->get_bit_array();
          dlog(fmt::format("sending pol: height={} round={}", prs->height, prs->round));
          transmit_new_envelope("", ps->peer_id, p2p::proposal_pol_message{rs->height, rs->proposal->pol_round, p_pol});
        }
        continue;
      }

      // Nothing to do, so just sleep for a while
      std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration); // TODO: check
    }
  });
}

void consensus_reactor::gossip_data_for_catchup(const std::shared_ptr<round_state>& rs,
  const std::shared_ptr<peer_round_state>& prs, const std::shared_ptr<peer_state>& ps) {
  if (auto [index, ok] = prs->proposal_block_parts->not_op()->pick_random(); ok) {
    // Verify peer's part_set_header
    block_meta block_meta_;
    if (!cs_state->block_store_->load_block_meta(prs->height, block_meta_)) {
      elog("failed to load block_meta");
      std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration); // TODO: check
      return;
    } else if (block_meta_.bl_id.parts != prs->proposal_block_part_set_header) {
      ilog("peer proposal_block_part_set_header mismatch");
      std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration); // TODO: check
      return;
    }

    part part_;
    if (!cs_state->block_store_->load_block_part(prs->height, index, part_)) {
      elog("failed to load block_part");
      std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration); // TODO: check
      return;
    }

    dlog("sending block_part for catchup");
    transmit_new_envelope(
      "", ps->peer_id, p2p::block_part_message{prs->height, prs->round, part_.index, part_.bytes_, part_.proof_});
    return;
  }
  std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration); // TODO: check
}

void consensus_reactor::gossip_votes_routine(std::shared_ptr<peer_state> ps) {
  ps->strand->post([this, ps{std::move(ps)}]() {
    while (true) {
      if (!ps->is_running)
        return;

      auto rs = cs_state->get_round_state();
      auto prs = ps->get_round_state();

      // If height matches, send last_commit, prevotes, precommits
      if (rs->height == prs->height) {
        if (gossip_votes_for_height(rs, prs, ps))
          continue;
      }

      // Special catchup - if peer is lagged by 1, send last_commit
      if (prs->height != 0 && rs->height == prs->height + 1) {
        if (pick_send_vote(ps, *rs->last_commit)) {
          dlog("picked last_commit to send");
          continue;
        }
      }

      // Catchup logic - if peer is lagged by more than 1, send commit
      auto block_store_base = cs_state->block_store_->base();
      if (block_store_base > 0 && prs->height != 0 && rs->height >= prs->height + 2 &&
        prs->height >= block_store_base) {
        // Load block_commit for prs->height which contains precommit sig
        commit commit_;
        if (cs_state->block_store_->load_block_commit(prs->height, commit_)) {
          // if (pick_send_vote(ps, commit_)) { // TODO: uncomment
          //  dlog("picked catchup commit to send");
          //  continue;
          // }
        }
      }

      // Nothing to do, so just sleep for a while
      std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration); // TODO: check
    }
  });
}

bool consensus_reactor::gossip_votes_for_height(const std::shared_ptr<round_state>& rs,
  const std::shared_ptr<peer_round_state>& prs, const std::shared_ptr<peer_state>& ps) {
  // If there are last_commits to send
  if (prs->step == p2p::round_step_type::NewHeight) {
    // if (pick_send_vote(ps, rs->last_commit)) {
    //  dlog("picked last_commit to send");
    //  return true;
    // }
  }
  // TODO
}

bool consensus_reactor::pick_send_vote(const std::shared_ptr<peer_state>& ps, const vote_set& votes_) {
  // TODO : implement, requires votes_ to handle VoteSetReader
}

/// \brief detect and react when there is a signature DDoS attack in progress
void consensus_reactor::query_maj23_routine(std::shared_ptr<peer_state> ps) {
  ps->strand->post([this, ps{std::move(ps)}]() {
    while (true) {
      if (!ps->is_running)
        return;

      // Send height/round/prevotes
      {
        auto rs = cs_state->get_round_state();
        auto prs = ps->get_round_state();
        if (rs->height == prs->height) {
          if (auto maj23 = rs->votes->prevotes(prs->round)->two_thirds_majority(); maj23.has_value()) {
            transmit_new_envelope(
              "", ps->peer_id, p2p::vote_set_maj23_message{prs->height, prs->round, p2p::Prevote, maj23.value()});
            std::this_thread::sleep_for(cs_state->cs_config.peer_query_maj_23_sleep_duration); // TODO: check
          }
        }
      }

      // Send height/round/Precommits
      {
        auto rs = cs_state->get_round_state();
        auto prs = ps->get_round_state();
        if (rs->height == prs->height) {
          if (auto maj23 = rs->votes->precommits(prs->round)->two_thirds_majority(); maj23.has_value()) {
            transmit_new_envelope(
              "", ps->peer_id, p2p::vote_set_maj23_message{prs->height, prs->round, p2p::Precommit, maj23.value()});
            std::this_thread::sleep_for(cs_state->cs_config.peer_query_maj_23_sleep_duration); // TODO: check
          }
        }
      }

      // Send height/round/proposal_pol
      {
        auto rs = cs_state->get_round_state();
        auto prs = ps->get_round_state();
        if (rs->height == prs->height && prs->proposal_pol_round >= 0) {
          if (auto maj23 = rs->votes->prevotes(prs->proposal_pol_round)->two_thirds_majority(); maj23.has_value()) {
            transmit_new_envelope("", ps->peer_id,
              p2p::vote_set_maj23_message{prs->height, prs->proposal_pol_round, p2p::Prevote, maj23.value()});
            std::this_thread::sleep_for(cs_state->cs_config.peer_query_maj_23_sleep_duration); // TODO: check
          }
        }
      }

      // Send height/catchup_commit_round/catchup_commit
      {
        auto prs = ps->get_round_state();
        if (prs->catchup_commit_round != -1 && prs->height > 0 && prs->height <= cs_state->block_store_->height() &&
          prs->height >= cs_state->block_store_->base()) {
          if (auto commit_ = cs_state->load_commit(prs->height); commit_ != nullptr) {
            transmit_new_envelope("", ps->peer_id,
              p2p::vote_set_maj23_message{prs->height, commit_->round, p2p::Precommit, commit_->my_block_id});
            std::this_thread::sleep_for(cs_state->cs_config.peer_query_maj_23_sleep_duration); // TODO: check
          }
        }
      }

      std::this_thread::sleep_for(cs_state->cs_config.peer_query_maj_23_sleep_duration); // TODO: check
    }
  });
}

void consensus_reactor::send_new_round_step_message(std::string peer_id) {
  auto rs = cs_state->get_round_state();
  auto msg = make_round_step_message(*rs);
  transmit_new_envelope("", peer_id, msg);
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
