// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/codec/rlp.h>
#include <noir/common/check.h>
#include <noir/common/hex.h>
#include <noir/eth/common/block.h>
#include <noir/eth/common/receipt.h>
#include <noir/eth/rpc/api.h>
#include <fmt/core.h>

namespace noir::eth::api {

using namespace noir::codec;

void api::check_params_size(const fc::variants& params, const uint32_t size) {
  check(!(params.size() < size), fmt::format("missing value for required argument {}", params.size()));
  check(!(params.size() > size), fmt::format("too many arguments, want at most {}", size));
}

void api::check_address(const std::string& address, const uint32_t index) {
  check(address.starts_with("0x"),
    fmt::format("invalid argument {}: json: cannot unmarshal hex string without 0x prefix", index));
  check(!(address.size() & 1),
    fmt::format("invalid argument {}: json: cannot unmarshal hex string of odd length into address", index));
  check(address.size() == 42,
    fmt::format("invalid argument {}: hex string has length {}, want 40 for address", index, (address.size() - 2)));
}

void api::check_hash(const std::string& hash, const uint32_t index) {
  check(hash.starts_with("0x"),
    fmt::format("invalid argument {}: json: cannot unmarshal hex string without 0x prefix", index));
  check(!(hash.size() & 1),
    fmt::format("invalid argument {}: json: cannot unmarshal hex string of odd length into hash", index));
  check(hash.size() == 66,
    fmt::format("invalid argument {}: hex string has length {}, want 64 for hash", index, (hash.size() - 2)));
}

fc::variant api::send_raw_tx(const fc::variant& req) {
  check(req.is_array(), "invalid json request");
  auto& params = req.get_array();
  check_params_size(params, 1);
  check(params[0].is_string(), "invalid parameters: json: cannot unmarshal");
  auto rlp = params[0].get_string();
  check(rlp.starts_with("0x"), "transaction could not be decoded: input must start with 0x");

  auto t = rlp::decode<transaction>(from_hex(rlp));
  check(tx_fee_cap == 0 || t.fee() <= tx_fee_cap, "tx fee exceeds the configured cap");
  if (!allow_unprotected_txs && !(t.v != 0 && t.v != 1 && t.v != 27 && t.v != 28)) {
    throw std::runtime_error("only replay-protected transactions allowed over RPC");
  }

  // TODO: add tx pool and return tx_hash
  consensus::tx tx(from_hex(rlp));
  tx_pool_ptr->check_tx_sync(tx);
  auto tx_hash = noir::consensus::get_tx_hash(tx);
  return fc::variant(fmt::format("0x{}", tx_hash.to_string()));
}

fc::variant api::chain_id(const fc::variant& req) {
  check(req.is_array() || req.is_null(), "invalid json request");
  // TODO: chain id
  return fc::variant("0xdeadbeef");
}

fc::variant api::net_version(const fc::variant& req) {
  check(req.is_array() || req.is_null(), "invalid json request");
  // TODO: net version
  return fc::variant("3735928559");
}

fc::variant api::net_listening(const fc::variant& req) {
  check(req.is_array() || req.is_null(), "invalid json request");
  return fc::variant(true);
}

fc::variant api::get_balance(const fc::variant& req) {
  check(req.is_array(), "invalid json request");
  auto params = req.get_array();
  check_params_size(params, 2);
  check(params[0].is_string(), "invalid argument 0: json: cannot unmarshal non-string");
  auto address = params[0].get_string();
  check_address(address, 0);
  check(params[1].is_string(), "invalid argument 1: json: cannot unmarshal non-string");
  auto block_number = params[1].get_string();
  check(block_number == "latest", "invalid argument 1: not supported block_number: only support latest");

  // TODO: get balance
  return fc::variant("0x0");
}

fc::variant api::get_tx_count(const fc::variant& req) {
  check(req.is_array(), "invalid json request");
  auto params = req.get_array();
  check_params_size(params, 2);
  check(params[0].is_string(), "invalid argument 0: json: cannot unmarshal non-string");
  auto address = params[0].get_string();
  check_address(address, 0);
  check(params[1].is_string(), "invalid argument 1: json: cannot unmarshal non-string");
  auto block_number = params[1].get_string();
  check(block_number == "latest", "invalid argument 1: not supported block_number: only support latest");

  // TODO: get tx count
  return fc::variant("0x0");
}

fc::variant api::block_number(const fc::variant& req) {
  check(req.is_array() || req.is_null(), "invalid json request");
  // TODO: block number
  auto block_number = block_store_ptr->height();
  return fc::variant(fmt::format("0x{}", to_hex((uint64_t)block_number)));
}

fc::variant api::gas_price(const fc::variant& req) {
  check(req.is_array() || req.is_null(), "invalid json request");
  // TODO: gas price
  return fc::variant("0x0");
}

fc::variant api::estimate_gas(const fc::variant& req) {
  check(req.is_array(), "invalid json request");
  // TODO: estimate gas
  return fc::variant("0x5208");
}

fc::variant api::get_tx_by_hash(const fc::variant& req) {
  check(req.is_array(), "invalid json request");
  auto& params = req.get_array();
  check_params_size(params, 1);
  check(params[0].is_string(), "invalid argument 0: json: cannot unmarshal non-string");
  auto hash = params[0].get_string();
  check_hash(hash, 0);
  // TODO: get tx by hash
  rpc_transaction t;
  return fc::variant(nullptr);
}

fc::variant api::get_block_by_number(const fc::variant& req) {
  check(req.is_array(), "invalid json request");
  auto params = req.get_array();
  check_params_size(params, 2);
  check(params[0].is_string(), "invalid argument 0: json: cannot unmarshal non-string");
  auto block_number = params[0].get_string();
  check(block_number == "earliest" || block_number == "pending" || block_number == "latest" ||
      block_number.starts_with("0x"),
    "invalid argument 0: json: cannot unmarshal hex string without 0x prefix");
  check(params[1].is_bool(), "invalid argument 1: json: cannot unmarshal into bool");
  auto full_tx = params[1].as_bool();
  uint64_t height;
  from_hex(block_number, height);
  consensus::block cb;
  if (block_store_ptr->load_block((int64_t)height, cb)) {
    // TODO: get block by number
    block b(cb);
    return fc::variant(b.to_json());
  } else {
    return fc::variant(nullptr);
  }
}

fc::variant api::get_block_by_hash(const fc::variant& req) {
  check(req.is_array(), "invalid json request");
  auto params = req.get_array();
  check_params_size(params, 2);
  check(params[0].is_string(), "invalid argument 0: json: cannot unmarshal non-string");
  auto hash = params[0].get_string();
  check_hash(hash, 0);
  check(params[1].is_bool(), "invalid argument 1: json: cannot unmarshal into bool");
  auto full_tx = params[1].as_bool();
  consensus::block cb;
  if (block_store_ptr->load_block_by_hash(from_hex(hash), cb)) {
    // TODO: get block by hash
    block b(cb);
    return fc::variant(b.to_json());
  } else {
    return fc::variant(nullptr);
  }
}

fc::variant api::get_tx_receipt(const fc::variant& req) {
  check(req.is_array(), "invalid json request");
  auto& params = req.get_array();
  check_params_size(params, 1);
  check(params[0].is_string(), "invalid argument 0: json: cannot unmarshal non-string");
  auto hash = params[0].get_string();
  check_hash(hash, 0);
  // TODO: get tx receipt
  receipt r;
  return fc::variant(nullptr);
}

fc::variant api::call(const fc::variant& req) {
  check(req.is_array(), "invalid json request");
  // TODO: call
  return fc::variant("0x0");
}

} // namespace noir::eth::api
