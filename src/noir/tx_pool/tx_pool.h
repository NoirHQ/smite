// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/common/plugin_interface.h>
#include <noir/consensus/abci_types.h>
#include <noir/consensus/app_connection.h>
#include <noir/consensus/tx.h>
#include <noir/tx_pool/LRU_cache.h>
#include <noir/tx_pool/unapplied_tx_queue.h>
#include <appbase/application.hpp>
#include <fc/exception/exception.hpp>

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
  tstamp ttl_duration{0};
  uint64_t ttl_num_blocks = 0;
  uint64_t gas_price_bump = 1000;
};

class tx_pool : public appbase::plugin<tx_pool> {
public:
  using precheck_func = bool(const consensus::tx&);
  using postcheck_func = bool(const consensus::tx&, consensus::response_check_tx&);

  std::shared_ptr<consensus::app_connection> proxy_app_;

private:
  std::mutex mutex_;
  config config_;
  unapplied_tx_queue tx_queue_;
  LRU_cache<consensus::tx_hash, consensus::tx_ptr> tx_cache_;

  uint64_t block_height_ = 0;

  precheck_func* precheck_ = nullptr;
  postcheck_func* postcheck_ = nullptr;

  plugin_interface::egress::channels::transmit_message_queue::channel_type& xmt_mq_channel_;
  plugin_interface::incoming::channels::tp_reactor_message_queue ::channel_type::handle msg_handle_;

public:
  tx_pool(appbase::application& app);
  tx_pool(appbase::application& app,
    const config& cfg,
    std::shared_ptr<consensus::app_connection>& new_proxyApp,
    uint64_t block_height);

  virtual ~tx_pool() = default;

  APPBASE_PLUGIN_REQUIRES()
  void set_program_options(CLI::App& config) override;

  void plugin_initialize(const CLI::App& config);
  void plugin_startup();
  void plugin_shutdown();

  void set_precheck(precheck_func* precheck);
  void set_postcheck(postcheck_func* postcheck);

  consensus::response_check_tx& check_tx_sync(const consensus::tx_ptr& tx_ptr);
  void check_tx_async(const consensus::tx_ptr& tx);

  std::vector<std::shared_ptr<const consensus::tx>> reap_max_bytes_max_gas(uint64_t max_bytes, uint64_t max_gas);
  std::vector<std::shared_ptr<const consensus::tx>> reap_max_txs(uint64_t tx_count);
  void update(uint64_t block_height,
    const std::vector<consensus::tx_ptr>& block_txs,
    std::vector<consensus::response_deliver_tx> responses,
    precheck_func* new_precheck = nullptr,
    postcheck_func* new_postcheck = nullptr);

  size_t size() const;
  uint64_t size_bytes() const;
  bool empty() const;
  void flush();
  void flush_app_conn();

private:
  void check_tx_internal(const consensus::tx_hash& tx_hash, const consensus::tx_ptr& tx);
  void add_tx(const consensus::tx_hash& tx_id, const consensus::tx_ptr& tx_ptr, consensus::response_check_tx& res);
  void update_recheck_txs();
  void broadcast_tx(const consensus::tx& tx);
  void handle_msg(p2p::envelope_ptr msg);
};

enum tx_pool_exception {
  existed_tx_code = 0,
  tx_size_error_code = 1,
  bad_transaction_code = 2,
  override_fail_code = 3,
  full_pool_code = 4,
};

} // namespace noir::tx_pool

namespace fc {
FC_DECLARE_EXCEPTION(existed_tx_exception, noir::tx_pool::existed_tx_code, "Existed tx");
FC_DECLARE_EXCEPTION(tx_size_exception, noir::tx_pool::tx_size_error_code, "Tx size error");
FC_DECLARE_EXCEPTION(bad_trasaction_exception, noir::tx_pool::bad_transaction_code, "Bad transaction");
FC_DECLARE_EXCEPTION(override_fail_exception, noir::tx_pool::override_fail_code, "Nonce override fail");
FC_DECLARE_EXCEPTION(full_pool_exception, noir::tx_pool::full_pool_code, "Pool is full");
} // namespace fc
