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

struct vote_info {
  validator validator_; // todo - do we need another data structure?
  bool signed_last_block;
};

struct last_commit_info {
  int32_t round;
  std::vector<vote_info> votes;
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

struct request_end_block {
  int64_t height;
};

struct response_commit {
  Bytes data;
  int64_t retain_height;
};

struct request_prepare_proposal {
  std::vector<Bytes> block_data;
  int64_t block_data_size;
  std::vector<std::shared_ptr<vote>> votes;
};

struct response_prepare_proposal {
  std::vector<Bytes> block_data;
};

struct request_deliver_tx {
  Bytes tx;
};

enum class check_tx_type {
  new_check = 0,
  recheck = 1,
};

struct request_check_tx {
  const consensus::tx& tx;
  check_tx_type type;
};

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::vote_info, validator_, signed_last_block);
NOIR_REFLECT(noir::consensus::last_commit_info, round, votes);
NOIR_REFLECT(noir::consensus::request_begin_block, hash, header_, last_commit_info_);
NOIR_REFLECT(noir::consensus::request_end_block, height);
NOIR_REFLECT(noir::consensus::response_commit, data, retain_height);
NOIR_REFLECT(noir::consensus::request_prepare_proposal, block_data, block_data_size, votes);
NOIR_REFLECT(noir::consensus::response_prepare_proposal, block_data);
NOIR_REFLECT(noir::consensus::request_deliver_tx, tx);
NOIR_REFLECT(noir::consensus::request_check_tx, tx, type);
