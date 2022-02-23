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

tx_pool::~tx_pool() {
  stop();
  thread_.reset();
}

void tx_pool::set_program_options(CLI::App& cfg) {
  auto tx_pool_options = cfg.add_section("tx_pool",
    "###############################################\n"
    "###      TX_POOL Configuration Options      ###\n"
    "###############################################");

  tx_pool_options->add_option("--thread_num", "A number of thread.")->default_val(5);
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

    config_.thread_num = tx_pool_options->get_option("--thread_num")->as<uint32_t>();
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
  start();
}

void tx_pool::plugin_shutdown() {
  ilog("Shutdown tx_pool");
  stop();
}

void tx_pool::start() {
  if (!is_running_) {
    thread_ = std::make_unique<named_thread_pool>("tx_pool", config_.thread_num);
    is_running_ = true;
  }
}

void tx_pool::stop() {
  if (is_running_) {
    thread_->stop();
    is_running_ = false;
  }
}

void tx_pool::set_precheck(precheck_func* precheck) {
  precheck_ = precheck;
}

void tx_pool::set_postcheck(postcheck_func* postcheck) {
  postcheck_ = postcheck;
}

std::optional<std::future<bool>> tx_pool::check_tx(const consensus::tx& tx, bool sync) {
  if (tx.size() > config_.max_tx_bytes) {
    return std::nullopt;
  }

  if (precheck_ && !precheck_(tx)) {
    return std::nullopt;
  }

  auto tx_id = consensus::get_tx_id(tx);

  if (tx_cache_.has(tx_id)) {
    // already in cache
    tx_cache_.put(tx_id, tx);
    return std::nullopt;
  }

  tx_cache_.put(tx_id, tx);

  auto result = async_thread_pool(thread_->get_executor(), [&, tx, tx_id]() {
    consensus::response_check_tx res = proxy_app_->check_tx_async(consensus::request_check_tx{.tx = tx});
    if (postcheck_ && !postcheck_(tx, res)) {
      if (res.code != consensus::code_type_ok) {
        tx_cache_.del(tx_id);
        return false;
      }
      // add tx to failed tx_pool
    }

    if (!res.sender.empty()) {
      // check tx_pool has sender
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto old = tx_queue_.get_tx(res.sender, res.nonce);
    if (old.has_value()) {
      auto old_tx = old.value();
      if (res.gas_wanted < old_tx->gas + config_.gas_price_bump) {
        if (!config_.keep_invalid_txs_in_cache) {
          tx_cache_.del(tx_id);
        }
        return false;
      }
      tx_queue_.erase(old_tx);
    }

    auto wtx = consensus::wrapped_tx{
      .sender = res.sender,
      ._id = tx_id,
      .tx_data = tx,
      .gas = res.gas_wanted,
      .nonce = res.nonce,
      .height = block_height_,
      .time_stamp = consensus::get_time(),
    };

    if (!tx_queue_.add_tx(std::make_shared<consensus::wrapped_tx>(wtx))) {
      if (!config_.keep_invalid_txs_in_cache) {
        tx_cache_.del(tx_id);
      }
      return false;
    }
    return true;
  });

  if (sync) {
    result.wait();
  }
  return result;
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
    txs.push_back(tx_ptr->tx_data);
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
    txs.push_back(itr->tx_ptr->tx_data);
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
      .tx = tx_ptr->tx_data,
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
