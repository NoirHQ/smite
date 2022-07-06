// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/clist/clist.h>
#include <noir/config/mempool.h>
#include <noir/core/core.h>
#include <noir/mempool/cache.h>
#include <noir/mempool/priority_queue.h>
#include <tendermint/proxy/app_conn.h>
#include <tendermint/types/tx.h>
#include <algorithm>

namespace noir::mempool {

struct ErrMempoolIsFull {
  int num_txs;
  int max_txs;
  int64_t txs_bytes;
  int64_t max_txs_bytes;

  std::string message() const {
		return fmt::format("mempool is full: number of txs {:d} (max: {:d}), total txs bytes {:d} (max: {:d})",
      num_txs, max_txs, txs_bytes, max_txs_bytes);
  }
};

template<abci::Client Client>
class TxMempool {
public:
  // with_pre_check();
  // with_post_check();
  // with_metrics();

  auto size() {
    return tx_store.size();
  }

  auto size_bytes() {
    return size_bytes_.load();
  }

  auto flush_app_conn() -> Result<void> {
    return proxy_app_conn->flush_sync();
  }

  auto wait_for_next_tx() -> chan<>& {
    return gossip_index.wait_chan();
  }

  auto next_gossip_tx() -> clist::CElementPtr<std::shared_ptr<WrappedTx>> {
    return gossip_index.front();
  }

  void enable_txs_available() {
    std::unique_lock _{mtx};
    txs_available_ = eo::make_chan(1);
  }

  auto txs_available() -> std::optional<chan<>>& {
    return txs_available_;
  }

  // check_tx();

  auto remove_tx_by_key(const types::TxKey& tx_key) -> Result<void> {
    std::unique_lock _{mtx};

    if (auto wtx = tx_store.get_tx_by_hash(tx_key); wtx) {
      remove_tx(wtx, false);
      return success();
    }

    return Error("transaction not found");
  }

  void flush() {
    std::shared_lock _{mtx};

    for (const auto& wtx : tx_store.get_all_txs()) {
      remove_tx(wtx, false);
    }

    size_bytes_ = 0;
    cache.reset();
  }

  auto reap_max_bytes_max_gas(int64_t max_bytes, int64_t max_gas) {
    std::shared_lock _{mtx};

    int64_t total_gas = 0;
    int64_t total_size = 0;

    auto wtxs = std::vector<std::shared_ptr<WrappedTx>>();
    wtxs.reserve(priority_index.num_txs());
    eo_defer([&]() {
      for (const auto& wtx : wtxs) {
        priority_index.push_tx(wtx);
      }
    });

    auto txs = std::vector<types::Tx>();
    txs.reserve(priority_index.num_txs());
    while (priority_index.num_txs() > 0) {
      auto wtx = priority_index.pop_tx();
      txs.push_back(wtx->tx);
      wtxs.push_back(wtx);
      // auto size = types::compute_proto_size_for_txs();
      auto size = 0;

      if (max_bytes > -1 && (total_size + size) > max_bytes) {
        // return txs...;
      }

      total_size += size;

      auto gas = total_gas + wtx->gas_wanted;
      if (max_gas > -1 && gas > max_gas) {
        // return txs...;
      }

      total_gas = gas;
    }

    return txs;
  }

  auto reap_max_txs(int max) {
    std::shared_lock _{mtx};

    auto num_txs = priority_index.num_txs();
    if (max < 0) {
      max = num_txs;
    }

    auto cap = std::min(num_txs, max);

    auto wtxs = std::vector<std::shared_ptr<WrappedTx>>();
    wtxs.reserve(cap);
    eo_defer([&]() {
      for (const auto& wtx : wtxs) {
        priority_index.push_tx(wtx);
      }
    });

    // XXX: consider returning pointers not to copy all txs
    auto txs = std::vector<types::Tx>();
    txs.reserve(cap);
    while (priority_index.num_txs() > 0 && txs.size() < max) {
      auto wtx = priority_index.pop_tx();
      txs.push_back(wtx->tx);
      wtxs.push_back(wtx);
    }

    return txs;
  }

  // auto update();

private:
  // void init_tx_callback(const std::shared_ptr<WrappedTx>& wtx, ...);

  // void default_tx_callback();

  // TODO
  void update_re_check_txs() {
    if (!size()) {
      throw std::runtime_error("attempted to update re-check_tx txs when mempool is empty");
    }

    recheck_cursor = gossip_index.front();
    recheck_end = gossip_index.back();
    //auto ctx = context::background();

    for (auto e = gossip_index.front(); e; e = e->next()) {
      if (!tx_store.is_tx_removed(e->value->hash)) {
        abci::RequestCheckTx req{};
        req.set_tx(e->tx);
        req.set_type(abci::CheckTxType::RECHECK);
        if (auto ok = invoke(proxy_app_conn->check_tx_async(req)); !ok) {
          // logger->error(...);
        }
      }
    }

    if (auto ok = invoke(proxy_app_conn->flush_async()); !ok) {
      // logger->error(...);
    }
  }

  auto can_add_tx(const std::shared_ptr<WrappedTx>& wtx) -> Result<void, ErrMempoolIsFull> {
    auto num_txs = size();
    auto size_bytes = this->size_bytes();

    if (num_txs >= config->size || int64_t(wtx->size())+ size_bytes > config->max_txs_bytes) {
      return ErrMempoolIsFull{
        .num_txs = num_txs,
        .max_txs = config->size,
        .txs_bytes = size_bytes_,
        .max_txs_bytes = config->max_txs_bytes,
      };
    }

    return success();
  }

  void insert_tx(const std::shared_ptr<WrappedTx>& wtx) {
    tx_store.set_tx(wtx);
    priority_index.push_tx(wtx);

    auto gossip_el = gossip_index.push_back(wtx);
    wtx->gossip_el = std::move(gossip_el);

    size_bytes_ += wtx->size();
  }

  void remove_tx(const std::shared_ptr<WrappedTx>& wtx, bool remove_from_cache) {
    if (tx_store.is_tx_removed(wtx->hash)) {
      return;
    }

    tx_store.remove_tx(wtx);
    priority_index.remove_tx(wtx);

    gossip_index.remove(wtx->gossip_el);
    wtx->gossip_el->detach_prev();

    size_bytes_ -= wtx->size();

    if (remove_from_cache) {
      cache.remove(wtx->tx);
    }
  }

  void purge_expired_txs(int64_t block_height) {
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    auto expired_txs = std::map<types::TxKey, std::shared_ptr<WrappedTx>>();

    if (config->ttl_num_blocks > 0) {
      auto& height_index = priority_index.txs.template get<by_height>();
      for (const auto& wtx : height_index) {
        if ((block_height - wtx->height) > config->ttl_num_blocks) {
          expired_txs[wtx->tx.key()] = wtx;
        } else {
          break;
        }
      }
    }

    if (config->ttl_duration.count() > 0) {
      auto& timestamp_index = priority_index.txs.template get<by_timestamp>();
      for (const auto& wtx : timestamp_index) {
        if ((now - wtx->timestamp) > config->ttl_duration.count()) {
          expired_txs[wtx->tx.key()] = wtx;
        } else {
          break;
        }
      }
    }

    for (const auto& wtx : expired_txs) {
      remove_tx(wtx.second, false);
    }
  }

  void notify_txs_available() {
    if (!size()) {
      throw std::runtime_error("attempt to notify txs available but mempool is empty!");
    }

    if (txs_available_ && !notified_txs_available) {
      notified_txs_available = true;

      txs_available_->try_send(boost::system::error_code{}, std::monostate{});
    }
  }

private:
  // std::shared_ptr<log::Logger> logger;
  // metrics;
  // TODO: change this to reference unless nullable
  config::MempoolConfig* config = nullptr;
  std::shared_ptr<proxy::AppConnMempool<Client>> proxy_app_conn;

  std::optional<chan<>> txs_available_;
  bool notified_txs_available;

  int64_t height;

  std::atomic<int64_t> size_bytes_;

  LRUTxCache cache;

  TxStore tx_store;

  clist::CList<std::shared_ptr<WrappedTx>> gossip_index;

  clist::CElementPtr<std::shared_ptr<WrappedTx>> recheck_cursor;
  clist::CElementPtr<std::shared_ptr<WrappedTx>> recheck_end;

  struct by_height;
  struct by_timestamp;

  TxPriorityQueue<
    boost::multi_index::ordered_non_unique<
      boost::multi_index::tag<by_height>,
      boost::multi_index::key<&WrappedTx::height>,
      std::greater_equal<int64_t>>,
    boost::multi_index::ordered_non_unique<
      boost::multi_index::tag<by_timestamp>,
      boost::multi_index::key<&WrappedTx::timestamp>,
      std::greater_equal<tstamp>>
  > priority_index;

  std::shared_mutex mtx;
  // mempool::PreCheckFunc pre_check;
  // mempool::PostCheckFunc post_check;
};

} // namespace noir::mempool
