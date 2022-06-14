// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/common.h>
#include <noir/consensus/types/block.h>
#include <noir/consensus/types/params.h>
#include <noir/consensus/types/validator.h>

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

// using this when async abci api is called
template<typename T>
struct req_res {
  std::mutex mutex;
  std::function<void(T&)> callback = nullptr;
  T res;

  void set_callback(std::function<void(T&)> cb) {
    std::scoped_lock _(mutex);
    callback = std::move(cb);
  }

  void invoke_callback() {
    std::scoped_lock _(mutex);
    if (callback) {
      callback(res);
    }
  }
};

struct request_begin_block {
  Bytes hash;
  block_header header_;
  last_commit_info last_commit_info_;
  std::vector<std::shared_ptr<::tendermint::abci::Evidence>> byzantine_validators;
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
  Bytes data;
  int64_t retain_height;
};

struct request_init_chain {
  tstamp time;
  std::string chain_id;
  consensus_params consensus_params_;
  std::vector<validator_update> validators;
  Bytes app_state_bytes;
  int64_t initial_height;
};

struct response_init_chain {
  consensus_params consensus_params_;
  std::vector<validator_update> validators;
  Bytes app_hash;
};

struct request_prepare_proposal {
  std::vector<Bytes> block_data;
  int64_t block_data_size;
  std::vector<std::optional<vote>> votes;
};

struct response_prepare_proposal {
  std::vector<Bytes> block_data;
};

struct request_deliver_tx {
  Bytes tx;
};

struct response_deliver_tx {
  uint32_t code;
  Bytes data;
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
  const consensus::tx& tx;
  check_tx_type type;
};

struct response_check_tx {
  uint32_t code;
  Bytes data;
  std::string log;
  std::string info;
  uint64_t gas_wanted;
  uint64_t gas_used;
  std::vector<event> events;
  std::string codespace;
  address_type sender;
  int64_t priority;
  std::string mempool_error;
  uint64_t nonce;
};

struct tx_result {
  int64_t height;
  uint32_t index;
  Bytes tx;
  response_deliver_tx result;
};

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::event_attribute, key, value, index);
NOIR_REFLECT(noir::consensus::event, type, attributes);
NOIR_REFLECT(noir::consensus::tx_result, height, index, tx, result);
NOIR_REFLECT(noir::consensus::vote_info, validator_, signed_last_block);
NOIR_REFLECT(noir::consensus::last_commit_info, round, votes);
NOIR_REFLECT(noir::consensus::validator_update, power);
NOIR_REFLECT(noir::consensus::request_begin_block, hash, header_, last_commit_info_);
NOIR_REFLECT(noir::consensus::response_begin_block, events);
NOIR_REFLECT(noir::consensus::request_end_block, height);
NOIR_REFLECT(noir::consensus::response_end_block, validator_updates, consensus_param_updates, events);
NOIR_REFLECT(noir::consensus::response_commit, data, retain_height);
NOIR_REFLECT(
  noir::consensus::request_init_chain, time, chain_id, consensus_params_, validators, app_state_bytes, initial_height);
NOIR_REFLECT(noir::consensus::response_init_chain, consensus_params_, validators, app_hash);
NOIR_REFLECT(noir::consensus::request_prepare_proposal, block_data, block_data_size, votes);
NOIR_REFLECT(noir::consensus::response_prepare_proposal, block_data);
NOIR_REFLECT(noir::consensus::request_deliver_tx, tx);
NOIR_REFLECT(noir::consensus::response_deliver_tx, code, data, log, info, gas_wanted, gas_used, events, codespace);
NOIR_REFLECT(noir::consensus::abci_responses, deliver_txs, end_block, begin_block);
NOIR_REFLECT(noir::consensus::request_check_tx, tx, type);
NOIR_REFLECT(noir::consensus::response_check_tx, code, data, log, info, gas_wanted, gas_used, events, codespace, sender,
  priority, mempool_error, nonce);
