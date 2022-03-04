// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/tx_pool/tx_pool.h>
#include <algorithm>

namespace noir::tx_pool {

tx_pool::tx_pool()
  : config_(config{}), tx_queue_(config_.max_tx_num * config_.max_tx_bytes), tx_cache_(config_.max_tx_num) {}

tx_pool::tx_pool(const config& cfg, std::shared_ptr<consensus::app_connection>& new_proxy_app, uint64_t block_height)
  : config_(cfg), tx_queue_(config_.max_tx_num * config_.max_tx_bytes), tx_cache_(config_.max_tx_num),
    proxy_app_(new_proxy_app), block_height_(block_height) {}

void tx_pool::set_program_options(CLI::App& cfg) {
  auto tx_pool_options = cfg.add_section("tx_pool",
    "###############################################\n"
    "###      TX_POOL Configuration Options      ###\n"
    "###############################################");

  tx_pool_options->add_option("--max_tx_num", "The maximum number of tx that the pool can store.")->default_val(10000);
  tx_pool_options->add_option("--max_tx_bytes", "The maximum bytes a single tx can hold.")->default_val(1024 * 1024);
  tx_pool_options->add_option("--ttl_duration", "Time(us) until tx expires in the pool. If it is '0', tx never expires")
    ->default_val(0);
  tx_pool_options
    ->add_option("--ttl_num_blocks", "Block height until tx expires in the pool. If it is '0', tx never expires")
    ->default_val(0);
  tx_pool_options->add_option("--gas_price_bump", "The minimum gas price for nonce override.")->default_val(1000);
}

void tx_pool::plugin_initialize(const CLI::App& config) {
  ilog("Initialize tx_pool");
  try {
    auto tx_pool_options = config.get_subcommand("tx_pool");

    config_.max_tx_num = tx_pool_options->get_option("--max_tx_num")->as<uint64_t>();
    config_.max_tx_bytes = tx_pool_options->get_option("--max_tx_bytes")->as<uint64_t>();
    config_.ttl_duration = tx_pool_options->get_option("--ttl_duration")->as<p2p::tstamp>();
    config_.ttl_num_blocks = tx_pool_options->get_option("--ttl_num_blocks")->as<uint64_t>();
    config_.gas_price_bump = tx_pool_options->get_option("--gas_price_bump")->as<uint64_t>();
  }
  FC_LOG_AND_RETHROW()
}

void tx_pool::plugin_startup() {
  ilog("Start tx_pool");
}

void tx_pool::plugin_shutdown() {
  ilog("Shutdown tx_pool");
}

void tx_pool::set_precheck(precheck_func* precheck) {
  precheck_ = precheck;
}

void tx_pool::set_postcheck(postcheck_func* postcheck) {
  postcheck_ = postcheck;
}

bool tx_pool::check_tx_sync(const consensus::tx& tx) {
  try {
    auto tx_id = consensus::get_tx_id(tx);
    auto& res = proxy_app_->check_tx_sync(consensus::request_check_tx{.tx = tx});
    check_tx_internal(tx_id, tx);
    return add_tx(tx_id, tx, res);
  } catch (std::exception& e) {
    elog(fmt::format("{}", e.what()));
    return false;
  }
}

bool tx_pool::check_tx_async(const consensus::tx& tx) {
  try {
    auto tx_id = consensus::get_tx_id(tx);
    check_tx_internal(tx_id, tx);
    auto& req_res = proxy_app_->check_tx_async(consensus::request_check_tx{.tx = tx});
    req_res.set_callback(
      [&, tx_id, tx](consensus::response_check_tx& res) { add_tx(tx_id, tx, res); }, thread_->get_executor());
    return true;
  } catch (std::exception& e) {
    elog(fmt::format("{}", e.what()));
    return false;
  }
}

bool tx_pool::check_tx_internal(const consensus::tx_id_type& tx_id, const consensus::tx& tx) {
  check(tx.size() < config_.max_tx_bytes,
    fmt::format("tx size {} bigger than {} (tx_id: {})", tx_id.to_string(), tx.size(), config_.max_tx_bytes));

  if (precheck_) {
    check(precheck_(tx), fmt::format("tx failed precheck (tx_id: {})", tx_id.to_string()));
  }

  if (tx_cache_.has(tx_id)) {
    // if tx already in pool, then just update cache
    tx_cache_.put(tx_id, tx);
    check(!tx_queue_.has(tx_id), fmt::format("tx already exists in pool (tx_id: {})", tx_id.to_string()));
  } else {
    tx_cache_.put(tx_id, tx);
  }

  return true;
}

bool tx_pool::add_tx(const consensus::tx_id_type& tx_id, const consensus::tx& tx, consensus::response_check_tx& res) {
  if (postcheck_ && !postcheck_(tx, res)) {
    if (res.code != consensus::code_type_ok) {
      tx_cache_.del(tx_id);
      dlog(fmt::format("reject bad transaction (tx_id: {})", tx_id.to_string()));
      return false;
    }
  }

  std::lock_guard<std::mutex> lock(mutex_);
  auto old = tx_queue_.get_tx(res.sender, res.nonce);
  if (old.has_value()) {
    auto old_tx = old.value();
    if (res.gas_wanted < old_tx->gas + config_.gas_price_bump) {
      if (!config_.keep_invalid_txs_in_cache) {
        tx_cache_.del(tx_id);
      }
      dlog(
        fmt::format("gas price is not enough for nonce override (tx_id: {}, nonce: {})", tx_id.to_string(), res.nonce));
      return false;
    }
    tx_queue_.erase(old_tx);
  }

  auto wtx = consensus::wrapped_tx{
    .sender = res.sender,
    ._id = tx_id,
    .tx = tx,
    .gas = res.gas_wanted,
    .nonce = res.nonce,
    .height = block_height_,
    .time_stamp = consensus::get_time(),
  };

  if (!tx_queue_.add_tx(std::make_shared<consensus::wrapped_tx>(wtx))) {
    if (!config_.keep_invalid_txs_in_cache) {
      tx_cache_.del(tx_id);
    }
    dlog(fmt::format("add tx fail (tx_id: {})", tx_id.to_string()));
    return false;
  }
  return true;
}

std::vector<consensus::tx> tx_pool::reap_max_bytes_max_gas(uint64_t max_bytes, uint64_t max_gas) {
  std::lock_guard<std::mutex> lock(mutex_);

  std::vector<consensus::tx> txs;
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
    txs.push_back(tx_ptr->tx);
  }

  return txs;
}

std::vector<consensus::tx> tx_pool::reap_max_txs(uint64_t tx_count) {
  std::lock_guard<std::mutex> lock(mutex_);
  uint64_t count = std::min<uint64_t>(tx_count, tx_queue_.size());

  std::vector<consensus::tx> txs;
  for (auto itr = tx_queue_.begin(); itr != tx_queue_.end(); itr++) {
    if (txs.size() >= count) {
      break;
    }
    txs.push_back(itr->tx_ptr->tx);
  }

  return txs;
}

bool tx_pool::update(uint64_t block_height, const std::vector<consensus::tx>& block_txs,
  std::vector<consensus::response_deliver_tx> responses, precheck_func* new_precheck, postcheck_func* new_postcheck) {
  std::lock_guard<std::mutex> lock(mutex_);
  block_height_ = block_height;

  if (new_precheck) {
    precheck_ = new_precheck;
  }

  if (new_postcheck) {
    postcheck_ = new_postcheck;
  }

  size_t size = std::min(block_txs.size(), responses.size());
  for (auto i = 0; i < size; i++) {
    auto tx_id = consensus::get_tx_id(block_txs[i]);
    if (responses[i].code == consensus::code_type_ok) {
      tx_cache_.put(tx_id, block_txs[i]);
    } else if (!config_.keep_invalid_txs_in_cache) {
      tx_cache_.del(tx_id);
    }

    tx_queue_.erase(tx_id);
  }

  if (config_.ttl_num_blocks > 0) {
    uint64_t expired_block_height = block_height_ > config_.ttl_num_blocks ? block_height_ - config_.ttl_num_blocks : 0;
    std::for_each(tx_queue_.begin<unapplied_tx_queue::by_height>(0),
      tx_queue_.end<unapplied_tx_queue::by_height>(expired_block_height),
      [&](auto& itr) { tx_queue_.erase(itr.tx_ptr->id()); });
  }

  if (config_.ttl_duration > 0) {
    auto expired_time = consensus::get_time() - config_.ttl_duration;
    std::for_each(tx_queue_.begin<unapplied_tx_queue::by_time>(0),
      tx_queue_.end<unapplied_tx_queue::by_time>(expired_time), [&](auto& itr) { tx_queue_.erase(itr.tx_ptr->id()); });
  }

  if (config_.recheck) {
    update_recheck_txs();
  }

  return true;
}

void tx_pool::update_recheck_txs() {
  for (auto itr = tx_queue_.begin(); itr != tx_queue_.end(); itr++) {
    auto tx_ptr = itr->tx_ptr;
    proxy_app_->check_tx_async(consensus::request_check_tx{
      .tx = tx_ptr->tx,
      .type = consensus::check_tx_type::recheck,
    });
    proxy_app_->flush_async();
  }
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

void tx_pool::flush_app_conn() {
  proxy_app_->flush_sync();
}

} // namespace noir::tx_pool
