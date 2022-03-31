// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/overloaded.h>
#include <noir/consensus/consensus_reactor.h>
#include <noir/core/codec.h>

namespace noir::consensus {

void consensus_reactor::process_peer_update(plugin_interface::peer_status_info_ptr info) {
  dlog(fmt::format("peer update: peer_id={}, status={}", info->peer_id, p2p::peer_status_to_str(info->status)));
  std::scoped_lock g(mtx);
  switch (info->status) {
  case p2p::peer_status::up: {
    auto it = peers.find(info->peer_id);
    if (it == peers.end()) {
      auto ps = peer_state::new_peer_state(info->peer_id, thread_pool_gossip->get_executor());
      peers[info->peer_id] = ps;
      it = peers.find(info->peer_id);
    }
    if (!it->second->is_running) {
      it->second->is_running = true;

      // Start gossips for this peer
      gossip_data_routine(it->second);
      gossip_votes_routine(it->second);
      query_maj23_routine(it->second);

      if (!wait_sync)
        send_new_round_step_message(info->peer_id);
    }
    break;
  }
  case p2p::peer_status::down: {
    auto it = peers.find(info->peer_id);
    if (it != peers.end() && it->second->is_running) {
      it->second->is_running = false;
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
  auto to = info->broadcast ? "all" : info->to;

  if (wait_sync) {
    dlog(fmt::format(
      "ignore messages received during sync. from={}, to={}, broadcast={}", from, to, info->broadcast));
    return;
  }

  datastream<char> ds(info->message.data(), info->message.size());
  p2p::cs_reactor_message msg;
  ds >> msg;

  dlog(fmt::format(
    "[cs_reactor] recv msg. from={}, to={}, type={}, broadcast={}", from, to, msg.index(), info->broadcast));
  auto ps = get_peer_state(from);
  if (!ps) {
    wlog(fmt::format("unable to find peer_state for from='{}'", from));
    return;
  }

  std::visit(
    overloaded{///
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
        if (auto err = votes->set_peer_maj23(msg.round, msg.type, ps->peer_id, msg.block_id_); err.has_value()) {
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
        internal_mq_channel.publish(
          appbase::priority::medium, std::make_shared<p2p::internal_msg_info>(p2p::internal_msg_info{msg, from}));
      },
      [&ps](p2p::proposal_pol_message& msg) { ps->apply_proposal_pol_message(msg); },
      [this, &ps, &from](p2p::block_part_message& msg) {
        ps->set_has_proposal_block_part(msg.height, msg.round, msg.index);
        internal_mq_channel.publish(
          appbase::priority::medium, std::make_shared<p2p::internal_msg_info>(p2p::internal_msg_info{msg, from}));
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

        internal_mq_channel.publish(
          appbase::priority::medium, std::make_shared<p2p::internal_msg_info>(p2p::internal_msg_info{msg, from}));
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
    if (!ps->is_running)
      return;

    auto rs = cs_state->get_round_state();
    auto prs = ps->get_round_state();

    int index{};
    bool ok{false};
    if (rs->proposal_block_parts->has_header(prs->proposal_block_part_set_header)) {
      std::tie(index, ok) =
        rs->proposal_block_parts->get_bit_array()->sub(prs->proposal_block_parts->copy())->pick_random();
    }
    if (ok) {
      // Send proposal_block_parts
      auto part = rs->proposal_block_parts->get_part(index);
      dlog(fmt::format("sending block_part: height={} round={}", prs->height, prs->round));
      transmit_new_envelope(
        "", ps->peer_id, p2p::block_part_message{rs->height, rs->round, part->index, part->bytes_, part->proof_});
      ps->set_has_proposal_block_part(prs->height, prs->round, index);
      gossip_data_routine(ps);

    } else if (auto block_store_base = cs_state->block_store_->base();
               block_store_base > 0 && 0 < prs->height && prs->height < rs->height && prs->height >= block_store_base) {
      // If peer is on a previous height, help catching up
      // If we never received commit_message from peer, block_parts will not be initialized
      if (prs->proposal_block_parts == nullptr) {
        block_meta block_meta_;
        if (!cs_state->block_store_->load_block_meta(prs->height, block_meta_)) {
          elog("failed to load block_meta");
          std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration);
        } else {
          ps->init_proposal_block_parts(block_meta_.bl_id.parts);
        }
      } else {
        gossip_data_for_catchup(rs, prs, ps);
      }
      gossip_data_routine(ps);

    } else if (rs->height != prs->height || rs->round != prs->round) {
      // If height and round don't match
      std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration);
      gossip_data_routine(ps);

    } else if (rs->proposal != nullptr && !prs->proposal) {
      /// By here, height and round should match.
      /// Proposal block_parts were matched and sent if any were needed.
      // Send proposal
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
      gossip_data_routine(ps);

    } else {
      // Nothing to do, so just sleep for a while
      std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration);
      gossip_data_routine(ps);
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
      std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration);
      return;
    } else if (block_meta_.bl_id.parts != prs->proposal_block_part_set_header) {
      ilog("peer proposal_block_part_set_header mismatch");
      std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration);
      return;
    }

    part part_;
    if (!cs_state->block_store_->load_block_part(prs->height, index, part_)) {
      elog("failed to load block_part");
      std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration);
      return;
    }

    dlog("sending block_part for catchup");
    transmit_new_envelope(
      "", ps->peer_id, p2p::block_part_message{prs->height, prs->round, part_.index, part_.bytes_, part_.proof_});
    return;
  }
  std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration);
}

void consensus_reactor::gossip_votes_routine(std::shared_ptr<peer_state> ps) {
  ps->strand->post([this, ps{std::move(ps)}]() {
    if (!ps->is_running)
      return;

    auto rs = cs_state->get_round_state();
    auto prs = ps->get_round_state();
    commit commit_;

    if (rs->height == prs->height && gossip_votes_for_height(rs, prs, ps)) {
      // If height matches, send last_commit, prevotes, precommits
      gossip_votes_routine(ps);

    } else if ((prs->height != 0 && rs->height == prs->height + 1) &&
      pick_send_vote(ps, vote_set_reader(*rs->last_commit))) {
      // Special catchup - if peer is lagged by 1, send last_commit
      dlog("picked last_commit to send");
      gossip_votes_routine(ps);

    } else if (auto block_store_base = cs_state->block_store_->base();
               (block_store_base > 0 && prs->height != 0 && rs->height >= prs->height + 2 &&
                 prs->height >= block_store_base) &&
               cs_state->block_store_->load_block_commit(prs->height, commit_) &&
               pick_send_vote(ps, vote_set_reader(commit_))) {
      // Catchup logic - if peer is lagged by more than 1, send commit
      // Load block_commit for prs->height which contains precommit sig
      dlog("picked catchup commit to send");
      gossip_votes_routine(ps);

    } else {
      // Nothing to do, so just sleep for a while
      std::this_thread::sleep_for(cs_state->cs_config.peer_gossip_sleep_duration);
      gossip_votes_routine(ps);
    }
  });
}

bool consensus_reactor::gossip_votes_for_height(const std::shared_ptr<round_state>& rs,
  const std::shared_ptr<peer_round_state>& prs, const std::shared_ptr<peer_state>& ps) {
  // If there are last_commits to send
  if (prs->step == p2p::round_step_type::NewHeight) {
    if (pick_send_vote(ps, vote_set_reader(*rs->last_commit))) {
      dlog("picked last_commit to send");
      return true;
    }
  }

  // If there are pol_prevotes to send
  if (prs->step <= p2p::round_step_type::Propose && prs->round != -1 && prs->round <= rs->round &&
    prs->proposal_pol_round != -1) {
    if (auto pol_prevotes = rs->votes->prevotes(prs->proposal_pol_round); pol_prevotes != nullptr) {
      if (pick_send_vote(ps, vote_set_reader(*pol_prevotes))) {
        dlog("picked prevotes(proposal_pol_round) to send");
        return true;
      }
    }
  }

  // If there are prevotes to send
  if (prs->step <= p2p::round_step_type::PrevoteWait && prs->round != -1 && prs->round <= rs->round) {
    if (pick_send_vote(ps, vote_set_reader(*rs->votes->prevotes(prs->round)))) {
      dlog("picked prevotes(round) to send");
      return true;
    }
  }

  // If there are precommits to send
  if (prs->step <= p2p::round_step_type::PrecommitWait && prs->round != -1 && prs->round <= rs->round) {
    if (pick_send_vote(ps, vote_set_reader(*rs->votes->precommits(prs->round)))) {
      dlog("picked precommits(round) to send");
      return true;
    }
  }

  // If there are prevotes to send, b/c of valid_block
  if (prs->round != -1 && prs->round <= rs->round) {
    if (pick_send_vote(ps, vote_set_reader(*rs->votes->prevotes(prs->round)))) {
      dlog("picked prevotes(round) to send");
      return true;
    }
  }

  // If there are pol_prevotes to send
  if (prs->proposal_pol_round != -1) {
    if (auto pol_prevotes = rs->votes->prevotes(prs->proposal_pol_round); pol_prevotes != nullptr) {
      if (pick_send_vote(ps, vote_set_reader(*pol_prevotes))) {
        dlog("picked prevotes(proposal_pol_round) to send");
        return true;
      }
    }
  }

  return false;
}

bool consensus_reactor::pick_send_vote(const std::shared_ptr<peer_state>& ps, const vote_set_reader& votes_) {
  if (auto [vote_, ok] = ps->pick_vote_to_send(const_cast<vote_set_reader&>(votes_)); ok) {
    dlog("cs_reactor: sending vote message");
    transmit_new_envelope("", ps->peer_id, p2p::vote_message{*vote_});
    ps->set_has_vote(*vote_);
    return true;
  }
  return false;
}

/// \brief detect and react when there is a signature DDoS attack in progress
void consensus_reactor::query_maj23_routine(std::shared_ptr<peer_state> ps) {
  thread_pool_query_maj23->get_executor().post([this, ps{std::move(ps)}]() {
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
          std::this_thread::sleep_for(cs_state->cs_config.peer_query_maj_23_sleep_duration);
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
          std::this_thread::sleep_for(cs_state->cs_config.peer_query_maj_23_sleep_duration);
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
          std::this_thread::sleep_for(cs_state->cs_config.peer_query_maj_23_sleep_duration);
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
          std::this_thread::sleep_for(cs_state->cs_config.peer_query_maj_23_sleep_duration);
        }
      }
    }

    std::this_thread::sleep_for(cs_state->cs_config.peer_query_maj_23_sleep_duration);
    query_maj23_routine(ps);
  });
}

void consensus_reactor::send_new_round_step_message(std::string peer_id) {
  auto rs = cs_state->get_round_state();
  auto msg = make_round_step_message(*rs);
  transmit_new_envelope("", peer_id, msg);
}

void consensus_reactor::transmit_new_envelope(
  std::string from, std::string to, const p2p::cs_reactor_message& msg, bool broadcast, int priority) {
  auto dest = broadcast ? "all" : to;
  dlog(fmt::format("[cs_reactor] send msg: from={}, to={}, type={}, broadcast={}", from, dest, msg.index(), broadcast));
  auto new_env = std::make_shared<p2p::envelope>();
  new_env->from = from;
  new_env->to = to;
  new_env->broadcast = broadcast;
  new_env->id = p2p::Consensus;

  const uint32_t payload_size = encode_size(msg);
  new_env->message.resize(payload_size);
  datastream<char> ds(new_env->message.data(), payload_size);
  ds << msg;

  xmt_mq_channel.publish(priority, new_env);
}

} // namespace noir::consensus
