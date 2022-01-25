// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/hex.h>
#include <noir/common/thread_pool.h>
#include <noir/tx_pool/tx_pool.h>
#include <noir/tx_pool/unapplied_tx_queue.hpp>
#include <catch2/catch_all.hpp>

using namespace noir;
using namespace noir::consensus;
using namespace noir::tx_pool;

TEST_CASE("[tx_pool] Add tx/Erase tx", "[tx_pool][unapplied_tx_queue]") {
  unapplied_tx_queue tx_queue;

  const uint64_t tx_count = 10;
  tx_id_type id[tx_count] = {
    tx_id_type{to_hex("1")},
    tx_id_type{to_hex("2")},
    tx_id_type{to_hex("3")},
    tx_id_type{to_hex("4")},
    tx_id_type{to_hex("5")},
    tx_id_type{to_hex("6")},
    tx_id_type{to_hex("7")},
    tx_id_type{to_hex("8")},
    tx_id_type{to_hex("9")},
    tx_id_type{to_hex("10")},
  };
  uint64_t gas[tx_count] = {100, 200, 500, 800, 1500, 7000, 300, 400, 500, 500};
  uint64_t nonce[tx_count] = {9, 5, 6, 7, 8, 1, 2, 3, 10, 4};

  // Add tx
  for (auto i = 0; i < tx_count; i++) {
    tx tx{"user", id[i], gas[i], nonce[i]};
    CHECK(tx_queue.add_tx(std::make_shared<::tx>(std::move(tx))));
  }
  CHECK(tx_queue.size() == tx_count);

  SECTION("Add same tx id") {
    tx tx{"user", id[0], 0, 20};
    CHECK(tx_queue.add_tx(std::make_shared<::tx>(std::move(tx))) == false);
    CHECK(tx_queue.size() == tx_count);
    CHECK(tx_queue.get_tx(id[0]));
  }

  SECTION("Add same user & nonce tx") {
    tx tx{"user", tx_id_type{to_hex("11")}, 0, 1};
    CHECK(tx_queue.add_tx(std::make_shared<::tx>(std::move(tx))) == false);
    CHECK(tx_queue.size() == tx_count);
    CHECK(tx_queue.get_tx(tx_id_type{to_hex("11")}) == std::nullopt);
  }

  SECTION("Erase tx") {
    for (auto & i : id) {
      CHECK(tx_queue.erase(i));
    }
    CHECK(tx_queue.empty());

    for (auto & i : id) {
      CHECK(tx_queue.erase(i) == false);
    }
    CHECK(tx_queue.empty());
  }

  SECTION("Erase tx by iterator") {
    auto itr = tx_queue.begin();
    while(itr != tx_queue.end()) {
      CHECK(tx_queue.erase(itr->id()));
      itr++;
    }
    CHECK(tx_queue.empty());
  }
}

TEST_CASE("[tx_pool] Fully add tx", "[tx_pool][unapplied_tx_queue]") {
  uint64_t tx_count = 10000;
  uint64_t queue_size = (sizeof(unapplied_tx) + sizeof(tx)) * tx_count;
  auto tx_queue = std::make_unique<unapplied_tx_queue>(queue_size);

  for (uint64_t i = 0; i < tx_count; i++) {
    tx tx{"user", tx_id_type{to_hex(std::to_string(i))}, i, i};
    CHECK(tx_queue->add_tx(std::make_shared<::tx>(std::move(tx))));
  }
  CHECK(tx_queue->size() == tx_count);

  for (uint64_t i = 0; i < tx_count; i++) {
    CHECK(tx_queue->erase(tx_id_type{to_hex(std::to_string(i))}));
  }
  CHECK(tx_queue->empty());
}

TEST_CASE("Push/Pop tx", "[tx_pool]") {
  class tx_pool tp;
  tx tx1{"user", tx_id_type{to_hex(std::to_string(0))}, 0, 0};

  auto add_tx = [&tp](uint64_t count, uint64_t offset, bool sync = true) {
    std::vector<std::optional<consensus::abci::response_check_tx>> res_vec;
    for (uint64_t i = offset; i < count + offset; i++) {
      tx tx{"user", tx_id_type{to_hex(std::to_string(i))}, i, i};
      res_vec.push_back(std::move(tp.check_tx(std::make_shared<::tx>(tx), sync)));
    }
    return res_vec;
  };

  auto get_tx = [&tp](uint64_t count) { return tp.reap_max_txs(count); };

  SECTION("sync") {
    auto res = add_tx(100, 0);
    for (auto& r : res) {
      REQUIRE(r.has_value());
      CHECK(r.value().result.get());
    }

    // fail case : same tx_id
    auto res_failed = add_tx(100, 0);
    for (auto& r : res_failed) {
      REQUIRE(r.has_value());
      CHECK(r.value().result.get() == false);
    }

    auto tx_ptrs = get_tx(100);
    CHECK(tx_ptrs.size() == 100);
    CHECK(tp.size() == 0);
  }

  SECTION("async") {
    auto thread = std::make_unique<named_thread_pool>("test_thread", 4);

    SECTION("2 thread add") {
      std::atomic<bool> start = false;
      auto add_res1 = async_thread_pool(thread->get_executor(), [&add_tx, &start]() {
        while (!start.load(std::memory_order_seq_cst)) {
        } // wait thread 2
        return add_tx(500, 0, false);
      });

      auto add_res2 = async_thread_pool(thread->get_executor(), [&add_tx, &start]() {
        while (!start.load(std::memory_order_seq_cst)) {
        } // wait thread 1
        return add_tx(500, 500, false);
      });

      start.store(true, std::memory_order_seq_cst);
      add_res1.wait();
      add_res2.wait();

      auto res1 = add_res1.get();
      for (auto& r : res1) {
        REQUIRE(r.has_value());
        CHECK(r.value().result.get());
      }

      auto res2 = add_res2.get();
      for (auto& r : res2) {
        REQUIRE(r.has_value());
        CHECK(r.value().result.get());
      }

      CHECK(tp.size() == 1000);
    }

    SECTION("1 thread add / 1 thread get") {
      std::atomic<bool> start = false;
      std::atomic<bool> add_end = false;

      auto add_res = async_thread_pool(thread->get_executor(), [&add_tx, &start]() {
        while (!start.load(std::memory_order_seq_cst)) {
        } // wait getter thread
        return add_tx(1000, 0, false);
      });

      auto get_res = async_thread_pool(thread->get_executor(), [&get_tx, &start, &add_end]() {
        while (!start.load(std::memory_order_seq_cst)) {
        } // wait setter thread

        uint64_t get_count = 0;
        while (get_count < 1000 && !add_end.load(std::memory_order_acquire)) {
          auto res = get_tx(1000 - get_count);
          get_count += res.size();
        }
        return get_count;
      });

      start.store(true, std::memory_order_seq_cst);
      add_res.wait();

      auto res = add_res.get();
      for (auto& r : res) {
        REQUIRE(r.has_value());
        CHECK(r.value().result.get());
      }

      add_end.store(true, std::memory_order_seq_cst);

      CHECK(get_res.get() == 1000);
      CHECK(tp.size() == 0);
    }

    SECTION("1 thread add / 2 thread get") {
      std::atomic<bool> start = false;
      std::atomic<bool> add_end = false;

      auto add_res = async_thread_pool(thread->get_executor(), [&add_tx, &start]() {
        while (!start.load(std::memory_order_seq_cst)) {
        } // wait other thread
        return add_tx(1000, 0, false);
      });

      auto get_res1 = async_thread_pool(thread->get_executor(), [&get_tx, &start, &add_end]() {
        while (!start.load(std::memory_order_seq_cst)) {
        } // wait other thread

        uint64_t get_count = 0;
        while (get_count < 1000 && !add_end.load(std::memory_order_acquire)) {
          auto res = get_tx(1000 - get_count);
          get_count += res.size();
        }
        return get_count;
      });

      auto get_res2 = async_thread_pool(thread->get_executor(), [&get_tx, &start, &add_end]() {
        while (!start.load(std::memory_order_seq_cst)) {
        } // wait other thread

        uint64_t get_count = 0;
        while (get_count < 1000 && !add_end.load(std::memory_order_acquire)) {
          auto res = get_tx(1000 - get_count);
          get_count += res.size();
        }
        return get_count;
      });

      start.store(true, std::memory_order_seq_cst);
      add_res.wait();

      auto res = add_res.get();
      for (auto& r : res) {
        REQUIRE(r.has_value());
        CHECK(r.value().result.get());
      }

      add_end.store(true, std::memory_order_seq_cst);

      uint64_t get_count = 0;
      get_count += get_res1.get();
      get_count += get_res2.get();
      CHECK(get_count == 1000);
      CHECK(tp.size() == 0);
    }
  }
}
