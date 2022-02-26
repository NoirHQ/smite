// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/block.h>
#include <noir/consensus/crypto.h>
#include <noir/consensus/node_id.h>
#include <noir/consensus/params.h>
#include <noir/consensus/protocol.h>
#include <noir/consensus/validator.h>
#include <noir/consensus/vote.h>
#include <noir/p2p/protocol.h>
#include <noir/p2p/types.h>

#include <fmt/core.h>

namespace noir::consensus {

constexpr size_t max_chain_id_len{50};

struct genesis_validator {
  bytes address;
  pub_key pub_key_;
  int64_t power;
  std::string name;
};

struct genesis_doc {
  p2p::tstamp genesis_time;
  std::string chain_id;
  int64_t initial_height;
  std::optional<consensus_params> cs_params;
  std::vector<genesis_validator> validators;
  bytes app_hash;
  bytes app_state;

  bool validate_and_complete() {
    if (chain_id.empty()) {
      elog("genesis doc must include non-empty chain_id");
      return false;
    }
    if (chain_id.length() > max_chain_id_len) {
      elog(fmt::format("chain_id in genesis doc is too long (max={})", max_chain_id_len));
      return false;
    }
    if (initial_height < 0) {
      elog("initial_height cannot be negative");
      return false;
    }
    if (initial_height == 0)
      initial_height = 1;

    if (!cs_params.has_value()) {
      cs_params = consensus_params::get_default();
    } else {
      auto err = cs_params->validate_consensus_params();
      if (err.has_value()) {
        elog(err.value());
        return false;
      }
    }

    int i{0};
    for (auto& v : validators) {
      if (v.power == 0) {
        elog("genesis file cannot contain validators with no voting power");
        return false;
      }
      // todo - uncomment after implementing methods to derive address from pub_key
      // if (!v.address.empty() && v.pub_key_.address() != v.address) {
      //  elog("genesis doc contains address that does not match its pub_key.address");
      //  return false;
      //}
      if (v.address.empty())
        validators[i].address = v.pub_key_.address();
      i++;
    }

    if (genesis_time == 0)
      genesis_time = std::chrono::system_clock::now().time_since_epoch().count();
    return true;
  }
};

struct round_vote_set {
  std::shared_ptr<vote_set> prevotes;
  std::shared_ptr<vote_set> precommits;
};

inline p2p::tstamp get_time() {
  return std::chrono::system_clock::now().time_since_epoch().count();
}

/**
 * Proposal defines a block proposal for the consensus.
 * It refers to the block by BlockID field.
 * It must be signed by the correct proposer for the given Height/Round
 * to be considered valid. It may depend on votes from a previous round,
 * a so-called Proof-of-Lock (POL) round, as noted in the POLRound.
 * If POLRound >= 0, then BlockID corresponds to the block that is locked in POLRound.
 */
struct proposal : p2p::proposal_message {

  static proposal new_proposal(int64_t height_, int32_t round_, int32_t pol_round_, p2p::block_id b_id_) {
    return proposal{p2p::Proposal, height_, round_, pol_round_, b_id_, get_time()};
  }
};

/// \brief SignedHeader is a header along with the commits that prove it.
struct signed_header {
  noir::consensus::block_header header;
  std::optional<noir::consensus::commit> commit;
};

struct weighted_time {
  p2p::tstamp time;
  int64_t weight;
};

inline p2p::tstamp weighted_median(std::vector<weighted_time>& weight_times, int64_t total_voting_power) {
  auto median = total_voting_power / 2;
  sort(weight_times.begin(), weight_times.end(), [](weighted_time a, weighted_time b) { return a.time < b.time; });
  p2p::tstamp res = 0;
  for (auto t : weight_times) {
    if (median <= t.weight) {
      res = t.time;
      break;
    }
    median -= t.weight;
  }
  return res;
}

} // namespace noir::consensus

NOIR_FOR_EACH_FIELD(std::chrono::system_clock::duration, )
NOIR_FOR_EACH_FIELD_DERIVED(noir::consensus::proposal, p2p::proposal_message)
