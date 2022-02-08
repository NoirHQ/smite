// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/tx_pool/tx_pool.h>

namespace noir::tx_pool {

tx_pool::tx_pool()
  : config_(config{}), tx_queue_(config_.pool_size * config_.max_tx_bytes), tx_cache_(config_.cache_size), precheck_(nullptr), postcheck_(nullptr),
    block_height_(0) {
  thread_ = std::make_unique<named_thread_pool>("tx_pool", config_.thread_num);
}

tx_pool::tx_pool(const config& cfg, uint64_t block_height)
  : config_(cfg), tx_queue_(config_.pool_size * config_.max_tx_bytes), tx_cache_(config_.cache_size), precheck_(nullptr), postcheck_(nullptr),
    block_height_(block_height) {
  thread_ = std::make_unique<named_thread_pool>("tx_pool", config_.thread_num);
}

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

  if (tx_cache_.has(tx_ptr->id())) {
    // already in cache
    tx_cache_.put(tx_ptr->id(), tx_ptr);
    return std::nullopt;
  }

  tx_cache_.put(tx_ptr->id(), tx_ptr);

  consensus::abci::response_check_tx res;
  res.result = async_thread_pool(thread_->get_executor(), [&, tx_ptr]() {
    if (postcheck_ && !postcheck_(*tx_ptr, res)) {
      if (res.code != consensus::abci::code_type_ok) {
        tx_cache_.del(tx_ptr->id());
        return false;
      }
      // add tx to failed tx_pool
    }

    if (!res.sender.empty()) {
      // check tx_pool has sender
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!tx_queue_.add_tx(tx_ptr)) {
      if (!config_.keep_invalid_txs_in_cache) {
        tx_cache_.del(tx_ptr->id());
      }
      return false;
    }
    return true;
  });

  if (sync) {
    res.result.wait();
  }
  return res;
}

consensus::tx_ptrs tx_pool::reap_max_bytes_max_gas(uint64_t max_bytes, uint64_t max_gas) {
  std::lock_guard<std::mutex> lock(mutex_);

  consensus::tx_ptrs tx_ptrs;
  auto rbegin = tx_queue_.rbegin<unapplied_tx_queue::by_gas>(max_gas);
  auto rend = tx_queue_.rend<unapplied_tx_queue::by_gas>(0);

  uint64_t bytes = 0;
  uint64_t gas = 0;
  for (auto itr = rbegin; itr != rend; itr++) {
    auto tx_ptr = itr->tx_ptr;
    if (gas + tx_ptr->gas > max_gas) {
      continue;
    }

    if (bytes + tx_ptr->size() > max_bytes) {
      break;
    }

    bytes += tx_ptr->size();
    gas += tx_ptr->gas;
    tx_ptrs.push_back(tx_ptr);
  }

  return tx_ptrs;
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
  }

  return tx_ptrs;
}

bool tx_pool::update(uint64_t block_height, consensus::tx_ptrs& block_txs,
  consensus::abci::response_deliver_txs& responses, precheck_func* new_precheck, postcheck_func* new_postcheck) {
  std::lock_guard<std::mutex> lock(mutex_);
  block_height_ = block_height;

  if (new_precheck) {
    precheck_ = new_precheck;
  }

  if (new_postcheck) {
    postcheck_ = new_postcheck;
  }

  size_t size = MIN(block_txs.size(), responses.size()); //
  for (auto i = 0; i < size; i++) {
    if (responses[i].code == consensus::abci::code_type_ok) {
      tx_cache_.put(block_txs[i]->id(), block_txs[i]);
    } else if (!config_.keep_invalid_txs_in_cache) {
      tx_cache_.del(block_txs[i]->id());
    }

    tx_queue_.erase(block_txs[i]->id());
  }

  if (config_.ttl_num_blocks > 0) {
    uint64_t expired_block_height = block_height_ - config_.ttl_num_blocks;
    std::for_each(tx_queue_.begin<unapplied_tx_queue::by_height>(0),
      tx_queue_.end<unapplied_tx_queue::by_height>(expired_block_height),
      [&](auto& itr) { tx_queue_.erase(itr.tx_ptr->id()); });
  }

  // TBD : clean expired tx by time

  return true;
}

size_t tx_pool::size() const {
  return tx_queue_.size();
}

bool tx_pool::empty() const {
  return tx_queue_.empty();
}

void tx_pool::flush() {
  std::lock_guard<std::mutex> lock_guard(mutex_);
  tx_queue_.clear();
  tx_cache_.reset();
}

} // namespace noir::tx_pool
