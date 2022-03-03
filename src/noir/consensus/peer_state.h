// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/node_id.h>
#include <noir/consensus/types.h>
#include <noir/consensus/types/peer_round_state.h>

#include <utility>

namespace noir::consensus {

struct peer_state {
  std::string peer_id;

  bool is_running{};
  peer_round_state prs;
  std::mutex mtx;
  std::shared_ptr<boost::asio::io_context::strand> strand;

  static std::shared_ptr<peer_state> new_peer_state(const std::string& peer_id_, boost::asio::io_context& ioc) {
    auto ret = std::make_shared<peer_state>();
    ret->peer_id = peer_id_;
    ret->prs.round = -1;
    ret->prs.proposal_pol_round = -1;
    ret->prs.last_commit_round = -1;
    ret->prs.catchup_commit_round = -1;
    ret->strand = std::make_shared<boost::asio::io_context::strand>(ioc);
    return ret;
  }

  void apply_new_round_step_message(const p2p::new_round_step_message& msg) {
    std::lock_guard<std::mutex> g(mtx);

    // Ignore duplicates or decreased values
    bool passed_checks{false};
    if (msg.height < prs.height)
      return;
    else if (msg.height > prs.height)
      passed_checks = true;
    if (!passed_checks && msg.round < prs.round)
      return;
    else if (msg.round > prs.round)
      passed_checks = true;
    if (!passed_checks && msg.step < prs.step)
      return;
    else if (msg.step > prs.step)
      passed_checks = true;
    if (!passed_checks)
      return;

    auto ps_height = prs.height;
    auto ps_round = prs.round;
    auto ps_catchup_commit_round = prs.catchup_commit_round;
    auto ps_catchup_commit = prs.catchup_commit;
    auto start_time = get_time() - msg.seconds_since_start_time; // TODO: check if this is correct

    prs.height = msg.height;
    prs.round = msg.round;
    prs.step = msg.step;
    prs.start_time = start_time;

    if (ps_height != msg.height || ps_round != msg.round) {
      prs.proposal = false;
      prs.proposal_block_part_set_header = p2p::part_set_header{};
      prs.proposal_block_parts = nullptr;
      prs.proposal_pol_round = -1;
      prs.proposal_pol = nullptr;
      prs.prevotes = nullptr;
      prs.precommits = nullptr;
    }

    if (ps_height == msg.height && ps_round != msg.round && msg.round == ps_catchup_commit_round) {
      // Peer caught up to catchup_commit_round so we need to preserve ps_catchup_commit
      prs.precommits = ps_catchup_commit;
    }

    if (ps_height != msg.height) {
      if (ps_height + 1 == msg.height && ps_round == msg.last_commit_round) {
        prs.last_commit_round = msg.last_commit_round;
        prs.last_commit = prs.precommits;
      } else {
        prs.last_commit_round = msg.last_commit_round;
        prs.last_commit = nullptr;
      }
      prs.catchup_commit_round = -1;
      prs.catchup_commit = nullptr;
    }
  }

  void apply_new_valid_block_message(const p2p::new_valid_block_message& msg) {
    std::lock_guard<std::mutex> g(mtx);
    if (prs.height != msg.height)
      return;
    if (prs.round != msg.round && !msg.is_commit)
      return;
    prs.proposal_block_part_set_header = msg.block_part_set_header;
    prs.proposal_block_parts = msg.block_parts;
  }

  void apply_proposal_pol_message(const p2p::proposal_pol_message& msg) {
    std::lock_guard<std::mutex> g(mtx);
    if (prs.height != msg.height)
      return;
    if (prs.proposal_pol_round != msg.proposal_pol_round)
      return;
    prs.proposal_pol = msg.proposal_pol;
  }

  void apply_has_vote_message(const p2p::has_vote_message& msg) {
    std::lock_guard<std::mutex> g(mtx);
    if (prs.height != msg.height)
      return;
    set_has_vote_(msg.height, msg.round, msg.type, msg.index);
  }

  void apply_vote_set_bits_message(const p2p::vote_set_bits_message& msg, std::shared_ptr<bit_array> our_votes) {
    std::lock_guard<std::mutex> g(mtx);
    auto votes = get_vote_bit_array(msg.height, msg.round, msg.type);
    if (!votes) {
      if (our_votes == nullptr) {
        votes->update(msg.votes);
      } else {
        auto other_votes = votes->sub(our_votes);
        auto has_votes = other_votes->or_op(msg.votes);
        votes->update(has_votes);
      }
    }
  }

  void set_has_proposal(const p2p::proposal_message& msg) {
    std::lock_guard<std::mutex> g(mtx);
    if (prs.height != msg.height || prs.round != msg.round)
      return;
    if (prs.proposal)
      return;
    prs.proposal = true;
    if (prs.proposal_block_parts != nullptr)
      return;
    prs.proposal_block_part_set_header = msg.block_id_.parts;
    prs.proposal_block_parts = bit_array::new_bit_array(msg.block_id_.parts.total);
    prs.proposal_pol_round = msg.pol_round;
    prs.proposal_pol = nullptr;
  }

  void set_has_proposal_block_part(int64_t height, int32_t round, int index) {
    std::lock_guard<std::mutex> g(mtx);
    if (prs.height != height || prs.round != round)
      return;
    prs.proposal_block_parts->set_index(index, true);
  }

private:
  void ensure_vote_bit_arrays_(int64_t height, int num_validators) {
    if (prs.height == height) {
      if (prs.prevotes == nullptr) {
        prs.prevotes = bit_array::new_bit_array(num_validators);
      }
      if (prs.precommits == nullptr) {
        prs.precommits = bit_array::new_bit_array(num_validators);
      }
      if (prs.catchup_commit == nullptr) {
        prs.catchup_commit = bit_array::new_bit_array(num_validators);
      }
      if (prs.proposal_pol == nullptr) {
        prs.proposal_pol = bit_array::new_bit_array(num_validators);
      }
    } else if (prs.height == height + 1) {
      if (prs.last_commit == nullptr) {
        prs.last_commit = bit_array::new_bit_array(num_validators);
      }
    }
  }

public:
  void ensure_vote_bit_arrays(int64_t height, int num_validators) {
    std::lock_guard<std::mutex> g(mtx);
    ensure_vote_bit_arrays_(height, num_validators);
  }

private:
  void set_has_vote_(int64_t height, int32_t round, p2p::signed_msg_type vote_type, int32_t index) {
    dlog(fmt::format("set_has_vote: type={} index={}", (int)vote_type, index));
    auto ps_votes = get_vote_bit_array(height, round, vote_type);
    if (!ps_votes)
      ps_votes->set_index(index, true);
  }

public:
  void set_has_vote(const p2p::vote_message& msg) {
    std::lock_guard<std::mutex> g(mtx);
    set_has_vote_(msg.height, msg.round, msg.type, msg.validator_index);
  }

  std::shared_ptr<bit_array> get_vote_bit_array(int64_t height, int32_t round, p2p::signed_msg_type vote_type) {
    // is_vote_type_valid // TODO:
    if (prs.height == height) {
      if (prs.round == round) {
        switch (vote_type) {
        case p2p::Prevote:
          return prs.prevotes;
        case p2p::Precommit:
          return prs.precommits;
        default:
          break;
        }
      }

      if (prs.catchup_commit_round == round) {
        switch (vote_type) {
        case p2p::Prevote:
          return {};
        case p2p::Precommit:
          return prs.catchup_commit;
        default:
          break;
        }
      }

      if (prs.proposal_pol_round == round) {
        switch (vote_type) {
        case p2p::Prevote:
          return prs.proposal_pol;
        case p2p::Precommit:
          return {};
        default:
          break;
        }
      }

      return {};
    }
    if (prs.height == height + 1) {
      if (prs.last_commit_round == round) {
        switch (vote_type) {
        case p2p::Prevote:
          return {};
        case p2p::Precommit:
          return prs.last_commit;
        default:
          break;
        }
      }
      return {};
    }
    return {};
  }

  std::shared_ptr<peer_round_state> get_round_state() {
    std::lock_guard<std::mutex> g(mtx);
    auto new_prs = std::make_shared<peer_round_state>();
    *new_prs = prs;
    return new_prs;
  }

  void init_proposal_block_parts(const p2p::part_set_header& part_set_header_) {
    std::lock_guard<std::mutex> g(mtx);
    if (prs.proposal_block_parts != nullptr)
      return;
    prs.proposal_block_part_set_header = part_set_header_;
    prs.proposal_block_parts = bit_array::new_bit_array(part_set_header_.total);
  }
};

} // namespace noir::consensus
