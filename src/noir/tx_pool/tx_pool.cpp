// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/tx_pool/tx_pool.h>

namespace noir::tx_pool {

tx_pool::tx_pool()
  : config_(config{}), tx_queue_(config_.cache_size * config_.max_tx_bytes), precheck_(nullptr), postcheck_(nullptr) {}

tx_pool::tx_pool(const config& cfg)
  : config_(cfg), tx_queue_(config_.cache_size * config_.max_tx_bytes), precheck_(nullptr), postcheck_(nullptr) {}

void tx_pool::set_precheck(precheck_func* precheck) {
  precheck_ = precheck;
}

void tx_pool::set_postcheck(postcheck_func* postcheck) {
  postcheck_ = postcheck;
}

bool tx_pool::check_tx(const consensus::tx_ptr& tx_ptr) {
  return true;
}

consensus::tx_ptrs tx_pool::reap_max_bytes_max_gas(uint64_t max_bytes, uint64_t max_gas) {
  return {};
}

consensus::tx_ptrs tx_pool::reap_max_txs(uint64_t tx_count) {
  return {};
}

bool tx_pool::update(uint64_t block_height, consensus::tx_ptrs& tx_ptrs) {
  return true;
}

size_t tx_pool::size() const {
}

void tx_pool::flush() {
}

} // namespace noir::tx_pool
