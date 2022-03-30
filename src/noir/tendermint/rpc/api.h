// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/tendermint/rpc/mempool.h>
#include <noir/tendermint/rpc/responses.h>
#include <fc/variant.hpp>

namespace noir::tendermint::rpc {
class api {
public:
  api(): mempool_(std::make_unique<rpc::mempool>()) {}

  fc::variant broadcast_tx_async(const fc::variant& req);
  fc::variant broadcast_tx_sync(const fc::variant& req);
  fc::variant broadcast_tx_commit(const fc::variant& req);
  fc::variant unconfirmed_txs(const fc::variant& req);
  fc::variant num_unconfirmed_txs(const fc::variant& req);
  fc::variant check_tx(const fc::variant& req);

  void set_tx_pool_ptr(noir::tx_pool::tx_pool* tx_pool_ptr) {
    mempool_->set_tx_pool_ptr(tx_pool_ptr);
  }

private:
  std::unique_ptr<rpc::mempool> mempool_;
};
} // namespace noir::tendermint::rpc
