// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/common/thread_pool.h>
#include <noir/consensus/abci_types.h>
#include <noir/consensus/app_connection.h>
#include <noir/consensus/tx.h>
#include <noir/tx_pool/LRU_cache.h>
#include <noir/tx_pool/unapplied_tx_queue.h>

namespace noir::tx_pool {
class tx_pool {
public:
  struct config {
    std::string version = "v0.0.1";
    std::string root_dir = "/";
    bool recheck = true;
    bool broadcast = true;
    uint32_t thread_num = 5;
    uint64_t max_tx_bytes = 1024 * 1024;
    uint64_t max_txs_bytes = 1024 * 1024 * 1024;
    uint64_t max_batch_bytes;
    uint32_t pool_size = 10000;
    uint64_t cache_size = 10000;
    bool keep_invalid_txs_in_cache = false;
    //    fc::time_point ttl_duration;
    uint64_t ttl_num_blocks = 0;
    uint64_t gas_price_bump = 1000;
  };

  using precheck_func = bool(const consensus::tx&);
  using postcheck_func = bool(const consensus::tx&, consensus::response_check_tx&);

private:
  std::mutex mutex_;
  config config_;
  unapplied_tx_queue tx_queue_;
  LRU_cache<consensus::tx_id_type, consensus::wrapped_tx_ptr> tx_cache_;

  std::shared_ptr<consensus::app_connection> proxy_app_;

  std::unique_ptr<named_thread_pool> thread_ = nullptr;

  uint64_t block_height_ = 0;

  precheck_func* precheck_ = nullptr;
  postcheck_func* postcheck_ = nullptr;

public:
  tx_pool();
  tx_pool(const config& cfg, std::shared_ptr<consensus::app_connection>& new_proxyApp, uint64_t block_height);

  ~tx_pool();

  void set_precheck(precheck_func* precheck);
  void set_postcheck(postcheck_func* postcheck);

  std::optional<std::future<bool>> check_tx(const consensus::wrapped_tx_ptr& tx_ptr, bool sync);
  consensus::wrapped_tx_ptrs reap_max_bytes_max_gas(uint64_t max_bytes, uint64_t max_gas);
  consensus::wrapped_tx_ptrs reap_max_txs(uint64_t tx_count);
  bool update(uint64_t block_height, consensus::wrapped_tx_ptrs& tx_ptrs,
    std::vector<consensus::response_deliver_tx> responses, precheck_func* new_precheck = nullptr,
    postcheck_func* new_postcheck = nullptr);

  size_t size() const;
  bool empty() const;
  void flush();
  void flush_app_conn() {
    proxy_app_->flush_sync();
  }

private:
  void update_recheck_txs();
};

} // namespace noir::tx_pool
