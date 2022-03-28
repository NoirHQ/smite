// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/plugin_interface.h>
#include <noir/common/thread_pool.h>
#include <noir/core/codec.h>
#include <noir/tx_pool/test/tx_pool_test_helper.h>
#include <noir/tx_pool/tx_pool.h>
#include <thread>

using namespace noir;
using namespace noir::consensus;
using namespace noir::tx_pool;
using namespace test_detail;

TEST_CASE("tx_pool: Add/Get tx", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  auto test_app = std::make_shared<test_application>();
  auto& tp = test_helper->make_tx_pool(test_app);

  SECTION("sync") {
    uint tx_count = 10;
    auto txs = test_helper->new_txs(tx_count);

    for (auto& tx : txs) {
      CHECK_NOTHROW(tp.check_tx_sync(tx));
    }

    // fail case : same txs
    for (auto& tx : txs) {
      CHECK_THROWS_AS(tp.check_tx_sync(tx), fc::existed_tx_exception);
    }

    CHECK(txs.size() == tx_count);
  }

  SECTION("async") {
    size_t thread_num = 10;
    auto thread = std::make_unique<named_thread_pool>("test_thread", thread_num);
    uint tx_count = 100;

    SECTION("multi thread add") {
      std::atomic<uint> token = thread_num;
      std::future<void> fs[thread_num];
      for (auto i = 0; i < thread_num; i++) {
        fs[i] = async_thread_pool(thread->get_executor(), [&]() {
          auto txs = test_helper->new_txs(tx_count);
          token.fetch_sub(1, std::memory_order_seq_cst);
          while (token.load(std::memory_order_seq_cst)) {
          } // wait other thread
          for (auto& tx : txs) {
            CHECK_NOTHROW(tp.check_tx_async(tx));
          }
        });
      }

      for (auto& f : fs) {
        f.get();
      }
      while (!test_app->is_empty_response()) {
      }

      CHECK(tp.size() == tx_count * thread_num);
    }

    SECTION("1 thread add / 1 thread get") {
      std::atomic<uint> token = 2;
      std::future<void> add_result;
      add_result = async_thread_pool(thread->get_executor(), [&]() {
        auto txs = test_helper->new_txs(tx_count);
        token.fetch_sub(1, std::memory_order_seq_cst);
        while (token.load(std::memory_order_seq_cst)) {
        } // wait other thread
        for (auto& tx : txs) {
          CHECK_NOTHROW(tp.check_tx_async(tx));
        }
      });

      std::future<uint> get_result;
      get_result = async_thread_pool(thread->get_executor(), [&]() {
        token.fetch_sub(1, std::memory_order_seq_cst);
        while (token.load(std::memory_order_seq_cst)) {
        } // wait other thread
        uint get_count = 0;
        auto start_time = get_time();
        while (get_count < tx_count) {
          auto txs = tp.reap_max_txs(tx_count - get_count);
          get_count += txs.size();
          if (get_time() - start_time >= 5000000 /* 5sec */) {
            break;
          }
        }
        return get_count;
      });

      add_result.get();
      while (!test_app->is_empty_response()) {
      }

      CHECK(get_result.get() == tx_count);
    }

    SECTION("2 thread add / 2 thread get") {
      std::atomic<uint> token = 4;
      std::future<void> add_result[2];

      for (auto& res : add_result) {
        res = async_thread_pool(thread->get_executor(), [&]() {
          auto txs = test_helper->new_txs(tx_count);
          token.fetch_sub(1, std::memory_order_seq_cst);
          while (token.load(std::memory_order_seq_cst)) {
          } // wait other thread
          for (auto& tx : txs) {
            CHECK_NOTHROW(tp.check_tx_async(tx));
          }
        });
      }

      std::future<uint> get_result[2];
      for (auto& res : get_result) {
        res = async_thread_pool(thread->get_executor(), [&]() {
          token.fetch_sub(1, std::memory_order_seq_cst);
          while (token.load(std::memory_order_seq_cst)) {
          } // wait other thread
          uint get_count = 0;
          auto start_time = get_time();
          while (get_count < tx_count) {
            auto txs = tp.reap_max_txs(tx_count - get_count);
            get_count += txs.size();
            if (get_time() - start_time >= 5000000 /* 5sec */) {
              break;
            }
          }
          return get_count;
        });
      }

      for (auto& res : add_result) {
        res.get();
      }
      while (!test_app->is_empty_response()) {
      }

      CHECK(get_result[0].get() + get_result[1].get() == tx_count * 2);
    }
  }
}

TEST_CASE("tx_pool: Reap tx using max bytes & gas", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  auto test_app = std::make_shared<test_application>();
  auto& tp = test_helper->make_tx_pool(test_app);

  const uint64_t tx_count = 10000;
  for (auto i = 0; i < tx_count; i++) {
    test_app->set_gas(test_detail::rand_gas());
    CHECK_NOTHROW(tp.check_tx_sync(test_helper->new_tx()));
  }

  uint tc = 100;
  for (uint i = 0; i < tc; i++) {
    uint64_t max_bytes = 1000;
    uint64_t max_gas = test_detail::rand_gas(1000000, 100000);
    auto txs = tp.reap_max_bytes_max_gas(max_bytes, max_gas);
    uint64_t total_bytes = 0;
    for (auto& tx : txs) {
      total_bytes += tx.size();
    }
    CHECK(total_bytes <= max_bytes);
  }
}

TEST_CASE("tx_pool: Update", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();

  const uint64_t tx_count = 10;
  auto put_tx = [&](class tx_pool& tp, std::vector<tx>& txs) {
    for (auto& tx : txs) {
      CHECK_NOTHROW(tp.check_tx_sync(tx));
    }
  };

  SECTION("Erase committed tx") {
    auto& tp = test_helper->make_tx_pool();
    auto txs = test_helper->new_txs(tx_count);
    put_tx(tp, txs);
    std::vector<consensus::response_deliver_tx> res;
    res.resize(tx_count);
    tp.update(0, txs, res);
    CHECK(tp.empty());
  }

  SECTION("Erase block height expired tx") {
    config config{
      .ttl_num_blocks = 10,
    };
    auto& tp = test_helper->make_tx_pool(config);
    std::vector<consensus::response_deliver_tx> res;
    std::vector<tx> empty_txs;
    for (uint64_t i = 0; i < tx_count; i++) {
      tp.update(i, empty_txs, res);
      auto txs = test_helper->new_txs(1);
      put_tx(tp, txs);
    }

    for (uint64_t i = 0; i < tx_count; i++) {
      tp.update(config.ttl_num_blocks + i, empty_txs, res);
      CHECK(tp.size() == tx_count - i - 1);
    }
    CHECK(tp.empty());
  }

  SECTION("Erase time expired tx") {
    config config{
      .ttl_duration = std::chrono::microseconds(100000LL).count(), // 100 msec
    };
    auto& tp = test_helper->make_tx_pool(config);
    auto txs = test_helper->new_txs(tx_count);
    put_tx(tp, txs);
    std::vector<consensus::response_deliver_tx> res;
    std::vector<consensus::tx> empty_txs;

    usleep(100000LL); // sleep 100 msec
    tp.update(0, empty_txs, res);
    CHECK(tp.empty());
  }
}

TEST_CASE("tx_pool: Nonce override", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  auto test_app = std::make_shared<test_application>();
  config config;
  auto& tp = test_helper->make_tx_pool(config, test_app);

  auto tx1 = test_helper->new_tx();
  CHECK_NOTHROW(tp.check_tx_sync(tx1));

  auto tx2 = test_helper->new_tx();
  test_app->set_nonce(0);
  CHECK_THROWS_AS(tp.check_tx_sync(tx2), fc::override_fail_exception);

  test_app->set_nonce(0);
  test_app->set_gas(config.gas_price_bump);
  CHECK_NOTHROW(tp.check_tx_sync(tx2));
}

TEST_CASE("tx_pool: Check Broadcast", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  config config{.broadcast = true};
  auto test_app = std::make_shared<test_application>();
  auto& tp = test_helper->make_tx_pool(config, test_app);

  std::atomic_bool result = false;
  consensus::tx tx;
  auto handle = app.get_channel<plugin_interface::egress::channels::transmit_message_queue>().subscribe(
    [&](const p2p::envelope_ptr& envelop) {
      CHECKED_IF(envelop->id == p2p::Transaction) {
        datastream<char> ds(envelop->message.data(), envelop->message.size());
        ds >> tx;
        result = true;
      }
    });

  auto thread = std::make_unique<named_thread_pool>("test_thread", 1);
  noir::async_thread_pool(thread->get_executor(), [&]() {
    app.register_plugin<test_plugin>();
    app.initialize<test_plugin>();
    app.startup();
    app.exec();
  });

  auto tx1 = test_helper->gen_random_tx();
  auto tx2 = test_helper->gen_random_tx();

  CHECK_NOTHROW(tp.check_tx_sync(tx1));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  CHECKED_IF(result) {
    CHECK(tx == tx1);
    result = false;
  }

  CHECK_NOTHROW(tp.check_tx_async(tx2));
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  CHECKED_IF(result) {
    CHECK(tx == tx2);
  }

  app.quit();
  thread->stop();
}
