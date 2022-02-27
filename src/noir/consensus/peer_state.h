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

  std::mutex mtx;
  bool is_running{};
  peer_round_state prs;

  static std::shared_ptr<peer_state> new_peer_state(const std::string& peer_id_) {
    auto ret = std::make_shared<peer_state>();
    ret->peer_id = peer_id_;
    peer_round_state prs_;
    ret->prs = prs_;
    return ret;
  }

  void apply_new_round_step_message(const p2p::new_round_step_message& msg) {
    // TODO:
  }

  void apply_new_valid_block_message(const p2p::new_valid_block_message& msg) {
    // TODO:
  }

  void apply_has_vote_message(const p2p::has_vote_message& msg) {
    // TODO:
  }

  void apply_vote_set_bits_message(const p2p::vote_set_bits_message& msg, std::shared_ptr<bit_array> our_votes) {
    // TODO:
  }

  void set_has_proposal(const p2p::proposal_message& msg) {
    // TODO:
  }

  void set_has_proposal_block_part(int64_t height, int32_t round, int index) {
    // TODO:
  }

  void ensure_vote_bit_arrays(int64_t height, int num_validators) {
    // TODO:
  }

  void set_has_vote(const p2p::vote_message& msg) {
    // TODO:
  }
};

} // namespace noir::consensus
