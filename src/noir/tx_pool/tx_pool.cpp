// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/tx_pool/tx_pool.h>

namespace noir::tx_pool {

tx_pool::tx_pool()
  : config_(config{}), tx_queue_(config_.cache_size * config_.max_tx_bytes), precheck_(nullptr), postcheck_(nullptr) {
  thread_ = std::make_unique<named_thread_pool>("tx_pool", 1);
}

tx_pool::tx_pool(const config& cfg)
  : config_(cfg), tx_queue_(config_.cache_size * config_.max_tx_bytes), precheck_(nullptr), postcheck_(nullptr) {}

tx_pool::~tx_pool() {
  thread_.reset();
}

void tx_pool::set_precheck(precheck_func* precheck) {
  precheck_ = precheck;
}

void tx_pool::set_postcheck(postcheck_func* postcheck) {
  postcheck_ = postcheck;
}

std::optional<consensus::abci::response_check_tx> tx_pool::check_tx(const consensus::tx_ptr& tx_ptr, bool sync) {
  if (tx_ptr->size() > config_.max_tx_bytes) {
    return std::nullopt;
  }

  if (precheck_ && !precheck_(*tx_ptr)) {
    return std::nullopt;
  }

  consensus::abci::response_check_tx res;
  res.result = async_thread_pool(thread_->get_executor(), [&res, tx_ptr, this](){
    if (postcheck_ && !postcheck_(*tx_ptr, res)) {
      if (res.code != consensus::abci::code_type_ok) {
        return false;
      }
      // add tx to failed tx_pool
    }

    if (!res.sender.empty()) {
      // check tx_pool has sender
    }

    std::lock_guard<std::mutex> lock(mutex_);
    return tx_queue_.add_tx(tx_ptr);
  });

  if (sync) {
    res.result.wait();
  }
  return res;
}

consensus::tx_ptrs tx_pool::reap_max_bytes_max_gas(uint64_t max_bytes, uint64_t max_gas) {
  return {};
}

consensus::tx_ptrs tx_pool::reap_max_txs(uint64_t tx_count) {
  std::lock_guard<std::mutex> lock(mutex_);
  uint64_t count = MIN(tx_count, tx_queue_.size());

  consensus::tx_ptrs tx_ptrs;
  for (auto itr = tx_queue_.begin(); itr != tx_queue_.end(); itr++) {
    if (tx_ptrs.size() >= count) {
      break;
    }
    tx_ptrs.push_back(itr->tx_ptr);
    tx_queue_.erase(itr->tx_ptr->id());
  }

  return tx_ptrs;
}

bool tx_pool::update(uint64_t block_height, consensus::tx_ptrs& tx_ptrs) {
  return true;
}

size_t tx_pool::size() const {
  return tx_queue_.size();
}

void tx_pool::flush() {
  std::lock_guard<std::mutex> lock_guard(mutex_);
  tx_queue_.clear();
}

} // namespace noir::tx_pool
