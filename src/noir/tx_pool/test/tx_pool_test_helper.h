// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/codec/scale.h>
#include <noir/common/thread_pool.h>
#include <noir/tx_pool/tx_pool.h>

using namespace noir;
using namespace noir::consensus;
using namespace noir::tx_pool;

namespace test_detail {

static appbase::application app;

static address_type str_to_addr(const std::string& str) {
  address_type addr(str.begin(), str.end());
  return addr;
}

static std::random_device random_device_;
static std::mt19937 generator{random_device_()};

static uint64_t rand_gas(uint64_t max = 0xFFFF, uint64_t min = 0) {
  std::uniform_int_distribution<uint64_t> dist_gas{min, max};
  return dist_gas(generator);
}

class test_application : public application::base_application {
  std::shared_mutex mutex_;
  std::list<std::shared_ptr<req_res<response_check_tx>>> rrs_;
  uint64_t nonce_ = 0;
  uint64_t gas_wanted_ = 0;

  std::unique_ptr<named_thread_pool> thread_;
  bool is_running = false;

public:
  test_application() {
    thread_ = std::make_unique<named_thread_pool>("test_application_thread", 1);
    is_running = true;

    async_thread_pool(thread_->get_executor(), [&]() {
      while (is_running) {
        handle_response();
        usleep(10);
      }
    });
  }

  ~test_application() {
    is_running = false;
    thread_->stop();
  }

  req_res<response_check_tx>& new_rr() {
    std::scoped_lock _(mutex_);
    response_check_tx res{
      .gas_wanted = gas_wanted_,
      .sender = str_to_addr("user"),
      .nonce = nonce_++,
    };

    auto rr_ptr = std::make_shared<req_res<response_check_tx>>();
    rr_ptr->res = std::make_shared<response_check_tx>(res);
    rrs_.push_back(rr_ptr);

    return *rr_ptr;
  }

  response_check_tx& check_tx_sync() override {
    return *new_rr().res;
  }

  req_res<response_check_tx>& check_tx_async() override {
    return new_rr();
  }

  void set_nonce(uint64_t nonce) {
    std::scoped_lock _(mutex_);
    nonce_ = nonce;
  }

  void set_gas(uint64_t gas) {
    std::scoped_lock lock(mutex_);
    gas_wanted_ = gas;
  }

  void handle_response() {
    if (rrs_.begin() != rrs_.end()) {
      auto& rr = rrs_.front();
      if (rr->callback != nullptr) {
        rr->invoke_callback();
        std::unique_lock _(mutex_);
        rrs_.pop_front();
      }
    }
  }

  void reset() {
    std::unique_lock _(mutex_);
    rrs_.clear();
    nonce_ = 0;
    gas_wanted_ = 0;
  }

  bool is_empty_response() {
    std::shared_lock _(mutex_);
    return rrs_.empty();
  }
};

class test_helper {
private:
  uint64_t tx_id_ = 0;
  uint64_t nonce_ = 0;
  uint64_t height_ = 0;

  std::mutex mutex_;

  std::shared_ptr<class tx_pool> tp_ = nullptr;

  std::shared_ptr<test_application> test_app_ = nullptr;

public:
  auto new_tx() {
    std::scoped_lock lock(mutex_);
    return codec::scale::encode(tx_id_++);
  }

  auto new_txs(uint count) {
    std::vector<consensus::tx> txs;
    txs.reserve(count);
    for (uint i = 0; i < count; i++) {
      txs.push_back(new_tx());
    }
    return txs;
  }

  auto make_random_wrapped_tx(const std::string& sender) {
    wrapped_tx new_wrapped_tx;
    new_wrapped_tx.sender = str_to_addr(sender);
    new_wrapped_tx.gas = rand_gas();

    std::scoped_lock lock(mutex_);
    new_wrapped_tx.tx = codec::scale::encode(tx_id_++);
    new_wrapped_tx.nonce = nonce_++;
    new_wrapped_tx.height = height_++;
    return std::make_shared<wrapped_tx>(new_wrapped_tx);
  }

  void reset_tx_id() {
    std::scoped_lock lock(mutex_);
    tx_id_ = 0;
  }

  class tx_pool& make_tx_pool() {
    config cfg{};
    test_app_ = std::make_shared<test_application>();
    return make_tx_pool(cfg, test_app_);
  }

  class tx_pool& make_tx_pool(config& cfg) {
    test_app_ = std::make_shared<test_application>();
    return make_tx_pool(cfg, test_app_);
  }

  class tx_pool& make_tx_pool(std::shared_ptr<test_application>& test_app) {
    config cfg{};
    return make_tx_pool(cfg, test_app);
  }

  class tx_pool& make_tx_pool(config& cfg, std::shared_ptr<test_application>& test_app) {
    auto proxy_app = std::make_shared<app_connection>();
    test_app_ = test_app;
    proxy_app->application = test_app;
    tp_ = std::make_shared<noir::tx_pool::tx_pool>(app, cfg, proxy_app, 0);
    return *tp_;
  }
};

} // namespace test_detail
