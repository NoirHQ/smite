// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <catch2/catch_all.hpp>
#include <noir/consensus/common_test.h>
#include <noir/consensus/ev/evidence_pool.h>
#include <noir/consensus/ev/test/evidence_test_common.h>
#include <noir/consensus/types/priv_validator.h>

using namespace noir;
using namespace noir::consensus;
using namespace noir::consensus::ev;

constexpr int64_t default_voting_power = 10;

Result<bool> sign_add_vote(
  const std::shared_ptr<priv_validator>& priv_val, vote& vote_, const std::shared_ptr<vote_set>& vote_set_) {
  auto v = vote::to_proto(vote_);
  if (auto ok = priv_val->sign_vote_pb(vote_set_->chain_id, *v); !ok)
    return ok.error();
  else
    vote_.signature = ok.value();
  auto [added, err] = vote_set_->add_vote(vote_);
  if (!err)
    return err;
  return added;
}

Result<std::shared_ptr<commit>> make_commit(const p2p::block_id& block_id_,
  int64_t height,
  int32_t round,
  std::shared_ptr<vote_set> vote_set_,
  const std::vector<std::shared_ptr<priv_validator>>& validators,
  tstamp now) {
  for (auto i = 0; i < validators.size(); i++) {
    auto pub_key_ = validators[i]->get_pub_key();
    vote vote_{{.type = noir::p2p::Precommit,
      .height = height,
      .round = round,
      .block_id_ = block_id_,
      .timestamp = now,
      .validator_address = pub_key_.address(),
      .validator_index = i}};
    if (auto ok = sign_add_vote(validators[i], vote_, vote_set_); !ok)
      return ok.error();
  }
  return vote_set_->make_commit();
}

std::vector<std::shared_ptr<priv_validator>> order_priv_vals_by_val_set(
  const std::shared_ptr<validator_set>& vals, const std::vector<std::shared_ptr<priv_validator>>& priv_vals) {
  std::vector<std::shared_ptr<priv_validator>> output(priv_vals.size());
  size_t idx{};
  for (auto& v : vals->validators) {
    for (auto& p : priv_vals) {
      auto pub_key_ = p->get_pub_key();
      if (v.address == pub_key_.address()) {
        output[idx] = p;
        break;
      }
    }
    idx++;
  }
  return output;
}

std::tuple<std::shared_ptr<light_client_attack_evidence>, std::shared_ptr<light_block>, std::shared_ptr<light_block>>
make_lunatic_evidence(int64_t height,
  int64_t common_height,
  int total_vals,
  int byz_vals,
  int phantom_vals,
  tstamp common_time,
  tstamp attack_time) {
  auto [common_val_set, common_priv_vals] = rand_validator_set(total_vals, default_voting_power);
  CHECK(total_vals > byz_vals);

  std::vector<validator> byz_val_set{
    common_val_set->validators.begin(), common_val_set->validators.begin() + byz_vals - 1};
  std::vector<std::shared_ptr<priv_validator>> byz_priv_vals{
    common_priv_vals.begin(), common_priv_vals.begin() + byz_vals - 1};

  auto [phantom_val_set, phantom_priv_vals] = rand_validator_set(phantom_vals, default_voting_power);

  auto conflicting_vals = phantom_val_set; // TODO: check
  conflicting_vals->update_with_change_set(byz_val_set, true);
  std::vector<std::shared_ptr<priv_validator>> conflicting_priv_vals;
  conflicting_priv_vals.insert(conflicting_priv_vals.end(), phantom_priv_vals.begin(), phantom_priv_vals.end());
  conflicting_priv_vals.insert(conflicting_priv_vals.end(), byz_priv_vals.begin(), byz_priv_vals.end());

  conflicting_priv_vals = order_priv_vals_by_val_set(conflicting_vals, conflicting_priv_vals);

  auto common_header =
    block_header::make_header({.chain_id = "test_chain", .height = common_height, .time = common_time});
  CHECK(common_header);
  auto trusted_header =
    block_header::make_header({.chain_id = "test_chain", .height = height, .time = get_default_evidence_time()});
  CHECK(trusted_header);
  auto conflicting_header = block_header::make_header(
    {.chain_id = "test_chain", .height = height, .time = attack_time, .validators_hash = conflicting_vals->get_hash()});
  CHECK(conflicting_header);

  auto block_id_ = p2p::block_id{.hash = random_hash(), .parts = {.total = 100, .hash = random_hash()}};
  auto vote_set_ = vote_set::new_vote_set("test_chain", height, 1, p2p::signed_msg_type::Precommit, conflicting_vals);
  auto commit_ = make_commit(block_id_, height, 1, vote_set_, conflicting_priv_vals, get_default_evidence_time());
  CHECK(commit_);

  auto ev = std::make_shared<light_client_attack_evidence>();
  ev->conflicting_block = std::make_shared<light_block>();
  ev->conflicting_block->s_header = std::make_shared<signed_header>();
  ev->conflicting_block->s_header->header = std::make_shared<consensus::block_header>(conflicting_header.value());
  ev->conflicting_block->s_header->commit = commit_.value();
  ev->conflicting_block->val_set = conflicting_vals;
  ev->common_height = common_height;
  ev->total_voting_power = common_val_set->get_total_voting_power();
  for (auto& v : byz_val_set) {
    ev->byzantine_validators.push_back(std::make_shared<validator>(v)); // TODO: convert to use shared_ptr
  }
  ev->timestamp = common_time;

  auto common = std::make_shared<light_block>();
  common->s_header = std::make_shared<signed_header>();
  common->s_header->header = std::make_shared<consensus::block_header>(common_header.value());
  common->s_header->commit = std::make_shared<commit>();
  common->val_set = common_val_set;

  auto trusted_block_id = p2p::block_id{.hash = random_hash(), .parts = {.total = 100, .hash = random_hash()}};
  auto [trusted_vals, priv_vals] = rand_validator_set(total_vals, default_voting_power);
  auto trusted_vote_set =
    vote_set::new_vote_set("test_chain", height, 1, p2p::signed_msg_type::Precommit, trusted_vals);
  auto trusted_commit =
    make_commit(trusted_block_id, height, 1, trusted_vote_set, priv_vals, get_default_evidence_time());
  CHECK(trusted_commit);
  auto trusted = std::make_shared<light_block>();
  trusted->s_header = std::make_shared<signed_header>();
  trusted->s_header->header = std::make_shared<consensus::block_header>(trusted_header.value());
  trusted->s_header->commit = trusted_commit.value();
  trusted->val_set = trusted_vals;

  return {ev, trusted, common};
}

TEST_CASE("evidence_verify: light client attack", "[noir][consensus]") {
  int64_t height{10}, common_height{4}, total_vals{10}, byz_vals{4};

  auto attack_time =
    get_default_evidence_time() + std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::hours(1)).count();
  // auto [ev, trusted, common] = make_lunatic_evidence(height, common_height, total_vals, byz_vals,
  // total_vals-byz_vals,
  //   get_default_evidence_time(), attack_time); // TODO: requires converting vote to only operate on pb
}
