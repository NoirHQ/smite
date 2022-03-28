// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/abci.h>
#include <noir/tx_pool/tx_pool.h>
#include <fc/variant.hpp>

namespace noir::eth::api {

class api {
public:
  fc::variant send_raw_tx(const fc::variant& req);
  fc::variant chain_id(const fc::variant& req);
  fc::variant net_version(const fc::variant& req);
  fc::variant net_listening(const fc::variant& req);
  fc::variant get_balance(const fc::variant& req);
  fc::variant get_tx_count(const fc::variant& req);
  fc::variant block_number(const fc::variant& req);
  fc::variant gas_price(const fc::variant& req);
  fc::variant estimate_gas(const fc::variant& req);
  fc::variant get_tx_by_hash(const fc::variant& req);
  fc::variant get_block_by_number(const fc::variant& req);
  fc::variant get_block_by_hash(const fc::variant& req);
  fc::variant get_tx_receipt(const fc::variant& req);
  fc::variant call(const fc::variant& req);

  static void check_params_size(const fc::variants& params, const uint32_t size);
  static void check_address(const std::string& address, const uint32_t index);
  static void check_hash(const std::string& hash, const uint32_t index);

  void set_tx_fee_cap(const uint256_t& tx_fee_cap) {
    this->tx_fee_cap = tx_fee_cap;
  }

  void set_allow_unprotected_txs(const bool& allow_unprotected_txs) {
    this->allow_unprotected_txs = allow_unprotected_txs;
  }

  void set_tx_pool_ptr(noir::tx_pool::tx_pool* tx_pool_ptr) {
    this->tx_pool_ptr = tx_pool_ptr;
  }

  void set_block_store(const std::shared_ptr<noir::consensus::block_store>& block_store_ptr) {
    this->block_store_ptr = block_store_ptr;
  }

private:
  uint256_t tx_fee_cap;
  bool allow_unprotected_txs;

  noir::tx_pool::tx_pool* tx_pool_ptr;
  std::shared_ptr<noir::consensus::block_store> block_store_ptr;
};

} // namespace noir::eth::api
