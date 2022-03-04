// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/consensus/abci_types.h>
#include <noir/consensus/app_connection.h>
#include <noir/consensus/tx.h>
#include <noir/tx_pool/LRU_cache.h>
#include <noir/tx_pool/unapplied_tx_queue.h>
#include <appbase/application.hpp>

namespace noir::tx_pool {

struct config {
  std::string version = "v0.0.1";
  std::string root_dir = "/";
  bool recheck = true;
  bool broadcast = true;
  uint64_t max_tx_bytes = 1024 * 1024;
  uint64_t max_txs_bytes = 1024 * 1024 * 1024;
  uint64_t max_batch_bytes;
  uint32_t max_tx_num = 10000;
  bool keep_invalid_txs_in_cache = false;
  p2p::tstamp ttl_duration{0};
  uint64_t ttl_num_blocks = 0;
  uint64_t gas_price_bump = 1000;
};

class tx_pool : public appbase::plugin<tx_pool> {
public:
  using precheck_func = bool(const consensus::tx&);
  using postcheck_func = bool(const consensus::tx&, consensus::response_check_tx&);

private:
  std::mutex mutex_;
  config config_;
  unapplied_tx_queue tx_queue_;
  LRU_cache<consensus::tx_id_type, consensus::tx> tx_cache_;

  std::shared_ptr<consensus::app_connection> proxy_app_;

  uint64_t block_height_ = 0;

  precheck_func* precheck_ = nullptr;
  postcheck_func* postcheck_ = nullptr;

public:
  tx_pool();
  tx_pool(const config& cfg, std::shared_ptr<consensus::app_connection>& new_proxyApp, uint64_t block_height);

  virtual ~tx_pool() = default;

  APPBASE_PLUGIN_REQUIRES()
  void set_program_options(CLI::App& config) override;

  void plugin_initialize(const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();

  void set_precheck(precheck_func* precheck);
  void set_postcheck(postcheck_func* postcheck);

  bool check_tx_sync(const consensus::tx& tx);
  bool check_tx_async(const consensus::tx& tx);

  std::vector<consensus::tx> reap_max_bytes_max_gas(uint64_t max_bytes, uint64_t max_gas);
  std::vector<consensus::tx> reap_max_txs(uint64_t tx_count);
  bool update(uint64_t block_height, const std::vector<consensus::tx>& block_txs,
    std::vector<consensus::response_deliver_tx> responses, precheck_func* new_precheck = nullptr,
    postcheck_func* new_postcheck = nullptr);

  size_t size() const;
  bool empty() const;
  void flush();
  void flush_app_conn();

private:
  bool check_tx_internal(const consensus::tx_id_type& tx_id, const consensus::tx& tx);
  bool add_tx(const consensus::tx_id_type& tx_id, const consensus::tx& tx, consensus::response_check_tx& res);
  void update_recheck_txs();
};

} // namespace noir::tx_pool
