// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/merkle/tree.h>
#include <noir/consensus/types.h>
#include <noir/consensus/types/light_block.h>
#include <tendermint/abci/types.pb.h>
#include <tendermint/types/evidence.pb.h>

#include <google/protobuf/util/time_util.h>

namespace noir::consensus {

struct evidence {
  virtual std::shared_ptr<::tendermint::abci::Evidence> get_abci() = 0;
  virtual bytes get_bytes() = 0;
  virtual bytes get_hash() = 0;
  virtual int64_t get_height() = 0;
  virtual std::string get_string() = 0;
  virtual tstamp get_timestamp() = 0;
  virtual result<void> validate_basic() = 0;
};

struct duplicate_vote_evidence : public evidence {
  std::shared_ptr<vote> vote_a{};
  std::shared_ptr<vote> vote_b{};
  int64_t total_voting_power{};
  int64_t validator_power{};
  tstamp timestamp{};

  static result<std::shared_ptr<duplicate_vote_evidence>> new_duplicate_vote_evidence(
    const std::shared_ptr<vote>& vote1,
    const std::shared_ptr<vote>& vote2,
    tstamp block_time,
    const std::shared_ptr<validator_set>& val_set) {
    if (!vote1 || !vote2)
      return make_unexpected("missing vote");
    if (!val_set)
      return make_unexpected("missing validator_set");
    auto val = val_set->get_by_address(vote1->validator_address);
    if (!val.has_value())
      return make_unexpected("validator is not in validator_set");
    std::shared_ptr<vote> vote_a_, vote_b_;
    if (vote1->block_id_.key() < vote2->block_id_.key()) {
      vote_a_ = vote1;
      vote_b_ = vote2;
    } else {
      vote_a_ = vote2;
      vote_b_ = vote1;
    }
    auto ret = std::make_shared<duplicate_vote_evidence>();
    ret->vote_a = vote_a_;
    ret->vote_b = vote_b_;
    ret->total_voting_power = val_set->total_voting_power;
    ret->validator_power = val->voting_power;
    ret->timestamp = block_time;
    return ret;
  }

  std::shared_ptr<::tendermint::abci::Evidence> get_abci() override {
    auto ret = std::make_shared<::tendermint::abci::Evidence>();
    ret->set_type(::tendermint::abci::DUPLICATE_VOTE);
    auto val = ret->mutable_validator();
    *val->mutable_address() = std::string(vote_a->validator_address.begin(), vote_a->validator_address.end());
    val->set_power(validator_power);
    ret->set_height(vote_a->height);
    auto ts = ret->mutable_time();
    *ts = ::google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(timestamp); // TODO: check if this is right
    ret->set_total_voting_power(total_voting_power);
    return ret;
  }

  bytes get_bytes() override {
    auto pbe = to_proto();
    bytes ret(pbe->ByteSizeLong());
    pbe->SerializeToArray(ret.data(), pbe->ByteSizeLong());
    return ret;
  }

  bytes get_hash() override {
    return crypto::sha256()(get_bytes());
  }

  int64_t get_height() override {
    return vote_a->height;
  }

  std::string get_string() override {
    return fmt::format("duplicate_vote_evidence{vote_a, vote_b}");
  }

  tstamp get_timestamp() override {
    return timestamp;
  }

  result<void> validate_basic() override {
    if (!vote_a || !vote_b)
      return make_unexpected("one or both of votes are empty");
    // if (vote_a->validate_basic)
    // if (vote_b->validate_basic)
    if (vote_a->block_id_.key() >= vote_b->block_id_.key())
      return make_unexpected("duplicate votes in invalid order");
    return {};
  }

  std::shared_ptr<::tendermint::types::DuplicateVoteEvidence> to_proto() {
    auto ret = std::make_shared<::tendermint::types::DuplicateVoteEvidence>();
    if (vote_b)
      *ret->mutable_vote_b() = *vote_b->to_proto();
    if (vote_a)
      *ret->mutable_vote_a() = *vote_a->to_proto();
    ret->set_total_voting_power(total_voting_power);
    ret->set_validator_power(validator_power);
    *ret->mutable_timestamp() = ::google::protobuf::util::TimeUtil::MicrosecondsToTimestamp(timestamp);
    return ret;
  }

  result<std::shared_ptr<duplicate_vote_evidence>> from_proto(
    std::shared_ptr<::tendermint::types::DuplicateVoteEvidence> pb) {
    if (!pb)
      return make_unexpected("null duplicate vote evidence");
    auto ret = std::make_shared<duplicate_vote_evidence>();
    return ret;
  }
};

struct light_client_attack_evidence : public evidence {
  std::shared_ptr<light_block> conflicting_block;
  int64_t common_height;

  // ABCI specific info
  std::vector<std::shared_ptr<validator>> byzantine_validators;
  int64_t total_voting_power;
  tstamp timestamp;

  std::shared_ptr<::tendermint::abci::Evidence> get_abci() override {}
  bytes get_bytes() override {}
  bytes get_hash() override {}
  int64_t get_height() override {}
  std::string get_string() override {}
  tstamp get_timestamp() override {}
  result<void> validate_basic() override {}

  std::shared_ptr<::tendermint::types::LightClientAttackEvidence> to_proto() {}
};

struct evidence_list {
  std::vector<std::shared_ptr<evidence>> list;

  bytes hash() {
    std::vector<bytes> bytes_list;
    for (auto& e : list)
      bytes_list.emplace_back(e->get_hash());
    return consensus::merkle::hash_from_bytes_list(bytes_list);
  }

  std::string to_string() {
    std::string ret;
    for (auto& e : list)
      ret += fmt::format("{}\t\t", e->get_string());
    return ret;
  }

  bool has(const std::shared_ptr<evidence>& ev) {
    for (auto& e : list) {
      if (e->get_hash() == ev->get_hash())
        return true;
    }
    return false;
  }
};

} // namespace noir::consensus
