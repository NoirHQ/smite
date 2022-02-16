// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/types.h>

namespace noir::consensus {

constexpr uint32_t code_type_ok{0};

struct event_attribute {
  std::string key;
  std::string value;
  bool index;
};

struct event {
  std::string type;
  std::vector<event_attribute> attributes;
};

struct vote_info {
  validator validator_; // todo - do we need another data structure?
  bool signed_last_block;
};

struct last_commit_info {
  int32_t round;
  std::vector<vote_info> votes;
};

struct validator_update {
  // pubkey
  int64_t power;

  static std::optional<std::vector<validator>> validator_updates(std::vector<validator_update>& vals) {
    std::vector<validator> tm_vals;
    for (auto v : vals) {
      pub_key pub; // todo - convert from proto
      tm_vals.push_back(validator::new_validator(pub, v.power));
    }
    return tm_vals;
  }
};

struct requst_begin_block {
  bytes hash;
  block_header header_;
  last_commit_info last_commit_info_;
  // evidence
};

struct response_begin_block {
  std::vector<event> events;
};

struct request_end_block {
  int64_t height;
};

struct response_end_block {
  std::vector<validator_update> validator_updates;
  std::optional<consensus_params> consensus_param_updates;
  std::vector<event> events;
};

struct response_commit {
  bytes data;
  int64_t retain_height;
};

struct request_extend_vote {
  vote vote_;
};

struct response_extend_vote {
  p2p::vote_extension vote_extension_;
};

struct request_verify_vote_extension {
  vote vote_;
};

struct response_verify_vote_extension {
  int32_t result;
};

struct request_init_chain {
  p2p::tstamp time;
  std::string chain_id;
  consensus_params consensus_params_;
  std::vector<validator_update> validators;
  bytes app_state_bytes;
  int64_t initial_height;
};

struct response_init_chain {
  consensus_params consensus_params_;
  std::vector<validator_update> validators;
  bytes app_hash;
};

struct request_prepare_proposal {
  std::vector<bytes> block_data;
  int64_t block_data_size;
  std::vector<vote> votes;
};

struct response_prepare_proposal {
  std::vector<bytes> block_data;
};

struct request_deliver_tx {
  bytes tx;
};

struct response_deliver_tx {
  uint32_t code;
  bytes data;
  std::string log;
  std::string info;
  int64_t gas_wanted;
  int64_t gas_used;
  std::vector<event> events;
  std::string codespace;
};

struct abci_responses {
  std::vector<response_deliver_tx> deliver_txs;
  response_end_block end_block;
  response_begin_block begin_block;
};

enum class check_tx_type {
  new_check = 0,
  recheck = 1,
};

struct request_check_tx {
  bytes tx;
  check_tx_type type;
};

struct response_check_tx {
  uint32_t code;
  bytes data;
  std::string log;
  std::string info;
  int64_t gas_wanted;
  int64_t gas_used;
  std::vector<event> events;
  std::string codespace;
  std::string sender;
  int64_t priority;
  std::string mempool_error;
};

} // namespace noir::consensus

NOIR_FOR_EACH_FIELD(noir::consensus::event_attribute, key, value, index)
NOIR_FOR_EACH_FIELD(noir::consensus::event, type, attributes)
NOIR_FOR_EACH_FIELD(noir::consensus::vote_info, validator_, signed_last_block)
NOIR_FOR_EACH_FIELD(noir::consensus::last_commit_info, round, votes)
NOIR_FOR_EACH_FIELD(noir::consensus::validator_update, power)
NOIR_FOR_EACH_FIELD(noir::consensus::requst_begin_block, hash, header_, last_commit_info_)
NOIR_FOR_EACH_FIELD(noir::consensus::response_begin_block, events)
NOIR_FOR_EACH_FIELD(noir::consensus::request_end_block, height)
NOIR_FOR_EACH_FIELD(noir::consensus::response_end_block, validator_updates, consensus_param_updates, events)
NOIR_FOR_EACH_FIELD(noir::consensus::response_commit, data, retain_height)
NOIR_FOR_EACH_FIELD(noir::consensus::request_extend_vote, vote_)
NOIR_FOR_EACH_FIELD(noir::consensus::response_extend_vote, vote_extension_)
NOIR_FOR_EACH_FIELD(noir::consensus::request_verify_vote_extension, vote_)
NOIR_FOR_EACH_FIELD(noir::consensus::response_verify_vote_extension, result)
NOIR_FOR_EACH_FIELD(
  noir::consensus::request_init_chain, time, chain_id, consensus_params_, validators, app_state_bytes, initial_height)
NOIR_FOR_EACH_FIELD(noir::consensus::response_init_chain, consensus_params_, validators, app_hash)
NOIR_FOR_EACH_FIELD(noir::consensus::request_prepare_proposal, block_data, block_data_size, votes)
NOIR_FOR_EACH_FIELD(noir::consensus::response_prepare_proposal, block_data)
NOIR_FOR_EACH_FIELD(noir::consensus::request_deliver_tx, tx)
NOIR_FOR_EACH_FIELD(
  noir::consensus::response_deliver_tx, code, data, log, info, gas_wanted, gas_used, events, codespace)
NOIR_FOR_EACH_FIELD(noir::consensus::abci_responses, deliver_txs, end_block, begin_block)
NOIR_FOR_EACH_FIELD(noir::consensus::request_check_tx, tx, type)
NOIR_FOR_EACH_FIELD(noir::consensus::response_check_tx, code, data, log, info, gas_wanted, gas_used, events, codespace,
  sender, priority, mempool_error)