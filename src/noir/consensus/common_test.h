// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/hex.h>
#include <noir/consensus/config.h>
#include <noir/consensus/consensus_state.h>
#include <noir/consensus/store/store_test.h>
#include <noir/consensus/types.h>

#include <fc/crypto/base58.hpp>
#include <fc/crypto/private_key.hpp>
#include <fc/crypto/public_key.hpp>
#include <fc/crypto/signature.hpp>

#include <stdlib.h>

appbase::application app;

namespace noir::consensus {

constexpr int64_t test_min_power = 10;

struct validator_stub {
  int32_t index;
  int64_t height;
  int32_t round;
  std::shared_ptr<priv_validator> priv_val;
  int64_t voting_power;
  vote last_vote;

  static validator_stub new_validator_stub(std::shared_ptr<priv_validator> priv_val_, int32_t val_index) {
    return validator_stub{val_index, 0, 0, priv_val_, test_min_power};
  }
};

using validator_stub_list = std::vector<validator_stub>;

void increment_height(validator_stub_list& vss, size_t begin_at) {
  for (auto it = vss.begin() + begin_at; it != vss.end(); it++)
    it->height++;
}

config config_setup() {
  auto config_ = config::get_default();
  config_.base.chain_id = "test_chain";

  // create consensus.root_dir if not exist
  auto temp_dir = std::filesystem::temp_directory_path();
  auto temp_config_path = temp_dir / "test_consensus";
  std::filesystem::create_directories(temp_config_path);

  config_.base.root_dir = temp_config_path.string();
  config_.consensus.root_dir = config_.base.root_dir;
  config_.priv_validator.root_dir = config_.base.root_dir;
  return config_;
}

std::tuple<validator, std::shared_ptr<priv_validator>> rand_validator(bool rand_power, int64_t min_power) {
  static int i = 0;
  auto priv_val = std::make_shared<mock_pv>();

  // Randomly generate a private key // TODO: use a different PKI algorithm
  // auto new_key = fc::crypto::private_key::generate<fc::ecc::private_key_shim>(); // randomly generate a private key
  fc::crypto::private_key new_key("5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3"); // TODO: read from a file
  priv_val->priv_key_.key = fc::from_base58(new_key.to_string());
  priv_val->pub_key_ = priv_val->priv_key_.get_pub_key();

  auto vote_power = min_power;
  if (rand_power)
    vote_power += std::rand();
  return {validator{priv_val->pub_key_.address(), priv_val->pub_key_, vote_power, 0}, priv_val};
}

std::tuple<genesis_doc, std::vector<std::shared_ptr<priv_validator>>> rand_genesis_doc(
  const config& config_, int num_validators, bool rand_power, int64_t min_power) {
  std::vector<genesis_validator> validators;
  std::vector<std::shared_ptr<priv_validator>> priv_validators;
  for (auto i = 0; i < num_validators; i++) {
    auto [val, priv_val] = rand_validator(rand_power, min_power);
    validators.push_back(genesis_validator{val.address, val.pub_key_, val.voting_power});
    priv_validators.push_back(priv_val);
  }
  return {genesis_doc{get_time(), config_.base.chain_id, 1, {}, validators}, priv_validators};
}

std::tuple<state, std::vector<std::shared_ptr<priv_validator>>> rand_genesis_state(
  config& config_, int num_validators, bool rand_power, int64_t min_power) {
  auto [gen_doc, priv_vals] = rand_genesis_doc(config_, num_validators, rand_power, min_power);
  auto state_ = state::make_genesis_state(gen_doc);
  return {state_, priv_vals};
}

std::tuple<std::shared_ptr<consensus_state>, validator_stub_list> rand_cs(config& config_, int num_validators) {
  auto [state_, priv_vals] = rand_genesis_state(config_, num_validators, false, 10);

  validator_stub_list vss;

  auto db_dir = std::filesystem::path{config_.consensus.root_dir} / "testdb";
  auto session = std::make_shared<noir::db::session::session<noir::db::session::rocksdb_t>>(make_session(true, db_dir));
  auto dbs = std::make_shared<noir::consensus::db_store>(session);
  auto proxyApp = std::make_shared<app_connection>();
  auto bls = std::make_shared<noir::consensus::block_store>(session);
  auto block_exec = block_executor::new_block_executor(dbs, proxyApp, bls);

  auto cs = consensus_state::new_state(app, config_.consensus, state_, block_exec, bls);
  cs->set_priv_validator(priv_vals[0]); // todo - requires many other fields to be properly initialized

  for (auto i = 0; i < num_validators; i++) {
    vss.push_back(validator_stub{i, 0, 0, priv_vals[i], test_min_power});
  }

  increment_height(vss, 1);

  return {move(cs), vss};
}

void start_test_round(std::shared_ptr<consensus_state>& cs, int64_t height, int32_t round) {
  cs->on_start();
  cs->enter_new_round(height, round);
  //  cs.startRoutines(0) // not needed for noir
}

void force_tick(std::shared_ptr<consensus_state>& cs) {
  cs->timeout_ticker_timer->cancel(); // forces tick
}

void sign_add_votes(config& config_, std::shared_ptr<consensus_state>& cs, p2p::signed_msg_type type, bytes hash,
  p2p::part_set_header header) {}

inline std::vector<noir::consensus::tx> make_txs(int64_t height, int num) {
  std::vector<noir::consensus::tx> txs{};
  for (auto i = 0; i < num; ++i) {
    txs.push_back(noir::consensus::tx{});
  }
  return txs;
}

inline std::shared_ptr<noir::consensus::block> make_block(
  int64_t height, const noir::consensus::state& st, const noir::consensus::commit& commit_) {
  auto txs = make_txs(st.last_block_height, 10);
  auto [block_, part_set_] = const_cast<noir::consensus::state&>(st).make_block(height, txs, commit_, /* {}, */ {});
  // TODO: temparary workaround to set block
  block_->header.height = height;
  return block_;
}

inline std::shared_ptr<noir::consensus::part_set> make_part_set(const noir::consensus::block& bl, uint32_t part_size) {
  // bl.mtx.lock();
  noir::p2p::part_set_header header_{
    .total = 1, // 4, // header, last_commit, data, evidence
  };
  auto p_set = noir::consensus::part_set::new_part_set_from_header(header_);

  p_set->add_part(std::make_shared<noir::consensus::part>(noir::consensus::part{
    .index = 0,
    .bytes_ = encode(bl.header), // .proof
  }));
  // TODO: part_size & bl.data
  // bl.mtx.unlock();
  return p_set;
}

inline noir::consensus::commit make_commit(int64_t height, noir::p2p::tstamp timestamp) {
  static const std::string sig_s("Signature");
  noir::consensus::commit_sig sig_{
    .flag = noir::consensus::block_id_flag::FlagCommit,
    .validator_address = gen_random_bytes(32),
    .timestamp = timestamp,
    .signature = {sig_s.begin(), sig_s.end()},
  };
  return {
    .height = height,
    .my_block_id = {.hash = gen_random_bytes(32),
      .parts =
        {
          .total = 2,
          .hash = gen_random_bytes(32),
        }},
    .signatures = {sig_},
  };
}

} // namespace noir::consensus
