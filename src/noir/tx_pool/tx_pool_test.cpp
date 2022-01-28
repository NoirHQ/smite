// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/hex.h>
#include <noir/common/thread_pool.h>
#include <noir/tx_pool/tx_pool.h>
#include <noir/tx_pool/unapplied_tx_queue.hpp>

using namespace noir;
using namespace noir::consensus;
using namespace noir::tx_pool;

namespace test_detail {

class test_helper {
private:
  uint64_t tx_id = 0;

  std::mutex mutex;

  std::random_device random_device;
  std::mt19937 generator{random_device()};
  std::uniform_int_distribution<uint16_t> dist_gas{0, std::numeric_limits<uint16_t>::max()};

public:
  auto make_random_tx(const sender_type& sender) {
    tx new_tx;
    new_tx.sender = sender;
    new_tx.gas = dist_gas(generator);

    std::lock_guard<std::mutex> lock(mutex);
    new_tx._id = tx_id_type{to_hex(std::to_string(tx_id))};
    new_tx.nonce = tx_id;
    tx_id++;
    return new_tx;
  }

  uint64_t get_tx_id() const {
    return tx_id;
  }

  void reset_tx_id() {
    std::lock_guard<std::mutex> lock(mutex);
    tx_id = 0;
  }
};

} // namespace test_detail

TEST_CASE("Add/Get/Erase tx", "[tx_pool][unapplied_tx_queue]") {
  auto test_helper = std::make_unique<test_detail::test_helper>();
  unapplied_tx_queue tx_queue;

  const uint64_t tx_count = 10;
  std::vector<tx> txs;

  for (auto i = 0; i < tx_count; i++) {
    txs.push_back(test_helper->make_random_tx("user"));
  }

  // Add tx
  for (auto i = 0; i < tx_count; i++) {
    CHECK(tx_queue.add_tx(std::make_shared<::tx>(std::move(txs[i]))));
  }
  CHECK(tx_queue.size() == tx_count);

  SECTION("Add same tx id") { // fail case
    auto tx = txs[0];
    tx.nonce = test_helper->get_tx_id() + 1;
    CHECK(tx_queue.add_tx(std::make_shared<::tx>(std::move(tx))) == false);
    CHECK(tx_queue.size() == tx_count);
    CHECK(tx_queue.get_tx(txs[0].id()));
    CHECK(tx_queue.get_tx("user"));
  }

  SECTION("Erase tx") {
    for (auto& tx : txs) {
      CHECK(tx_queue.erase(tx.id()));
    }
    CHECK(tx_queue.empty());

    // fail case
    for (auto& tx : txs) {
      CHECK(tx_queue.erase(tx.id()) == false);
    }
    CHECK(tx_queue.empty());
  }

  SECTION("Erase tx by iterator") {
    auto itr = tx_queue.begin();
    while (itr != tx_queue.end()) {
      CHECK(tx_queue.erase(itr->id()));
      itr++;
    }
    CHECK(tx_queue.empty());
  }
}

TEST_CASE("Fully add tx", "[tx_pool][unapplied_tx_queue]") {
  auto test_helper = std::make_unique<test_detail::test_helper>();
  uint64_t tx_count = 10000;
  uint64_t queue_size = (sizeof(unapplied_tx) + sizeof(tx)) * tx_count;
  auto tx_queue = std::make_unique<unapplied_tx_queue>(queue_size);

  for (uint64_t i = 0; i < tx_count; i++) {
    CHECK(tx_queue->add_tx(std::make_shared<::tx>(std::move(test_helper->make_random_tx("user")))));
  }
  CHECK(tx_queue->size() == tx_count);

  for (uint64_t i = 0; i < tx_count; i++) {
    CHECK(tx_queue->erase(tx_id_type{to_hex(std::to_string(i))}));
  }
  CHECK(tx_queue->empty());
}

TEST_CASE("Indexing", "[tx_pool][unapplied_tx_queue]") {
  auto test_helper = std::make_unique<test_detail::test_helper>();
  uint64_t tx_count = 10000;
  uint64_t user_count = tx_count / 100;
  uint64_t queue_size = noir::tx_pool::tx_pool::config{}.max_tx_bytes * tx_count;
  auto tx_queue = std::make_unique<unapplied_tx_queue>(queue_size);

  for (uint64_t i = 0; i < tx_count; i++) {
    sender_type sender = "user" + std::to_string(i / user_count);
    CHECK(tx_queue->add_tx(std::make_shared<::tx>(test_helper->make_random_tx(sender))));
  }
  CHECK(tx_queue->size() == tx_count);

  SECTION("by nonce") {
    auto begin = tx_queue->begin<unapplied_tx_queue::by_nonce>();
    auto end = tx_queue->end<unapplied_tx_queue::by_nonce>();

    uint64_t count = 0;
    for (auto itr = begin; itr != end; itr++) {
      count++;
    }
    CHECK(count == tx_count);
  }

  SECTION("a specific sender's txs") {
    SECTION("all txs") {
      uint64_t tx_count_per_user = tx_count / user_count;
      for (uint64_t i = 0; i < user_count; i++) {
        sender_type sender = "user" + std::to_string(i);
        auto begin = tx_queue->begin(sender);
        auto end = tx_queue->end(sender);

        uint64_t count = 0;
        for (auto itr = begin; itr != end; itr++) {
          count++;
        }
        CHECK(count == tx_count_per_user);
      }
    }
  }

  SECTION("ordering") {
    SECTION("ascending") {
      auto begin = tx_queue->begin<unapplied_tx_queue::by_gas>();
      auto end = tx_queue->end<unapplied_tx_queue::by_gas>();

      uint64_t prev_gas = 0;
      for (auto itr = begin; itr != end; itr++) {
        CHECK(itr->gas() >= prev_gas);
        prev_gas = itr->gas();
      }
    }

    SECTION("descending") {
      auto rbegin = tx_queue->rbegin<unapplied_tx_queue::by_gas>();
      auto rend = tx_queue->rend<unapplied_tx_queue::by_gas>();

      uint64_t prev_gas = std::numeric_limits<uint64_t>::max();
      for (auto itr = rbegin; itr != rend; itr++) {
        CHECK(itr->gas() <= prev_gas);
        prev_gas = itr->gas();
      }
    }
  }

  SECTION("bound") {
    uint64_t lowest = 1000;
    uint64_t highest = 50000;
    auto begin = tx_queue->begin<unapplied_tx_queue::by_gas>(lowest);
    auto end = tx_queue->end<unapplied_tx_queue::by_gas>(highest);

    for (auto itr = begin; itr != end; itr++) {
      CHECK(lowest <= itr->gas());
      CHECK(itr->gas() <= highest);
    }

    auto rbegin = tx_queue->rbegin<unapplied_tx_queue::by_gas>(highest);
    auto rend = tx_queue->rend<unapplied_tx_queue::by_gas>(lowest);

    for (auto itr = rbegin; itr != rend; itr++) {
      CHECK(lowest <= itr->gas());
      CHECK(itr->gas() <= highest);
    }
  }
}

TEST_CASE("Push/Pop tx", "[tx_pool]") {
  auto test_helper = std::make_unique<test_detail::test_helper>();
  class tx_pool tp;

  auto push_tx = [&tp, &test_helper](uint64_t count, uint64_t offset, bool sync = true) {
    std::vector<std::optional<consensus::abci::response_check_tx>> res_vec;
    for (uint64_t i = offset; i < count + offset; i++) {
      res_vec.push_back(std::move(tp.check_tx(std::make_shared<::tx>(test_helper->make_random_tx("user")), sync)));
    }
    return res_vec;
  };

  auto pop_tx = [&tp](uint64_t count) { return tp.reap_max_txs(count); };

  SECTION("sync") {
    auto res = push_tx(100, 0);
    for (auto& r : res) {
      CHECKED_IF(r.has_value()) {
        CHECK(r.value().result.get());
      }
    }

    // fail case : same tx_id
    test_helper->reset_tx_id();
    auto res_failed = push_tx(100, 0);
    for (auto& r : res_failed) {
      CHECKED_IF(r.has_value()) {
        CHECK(r.value().result.get() == false);
      }
    }

    auto tx_ptrs = pop_tx(100);
    CHECK(tx_ptrs.size() == 100);
    CHECK(tp.size() == 0);
  }

  SECTION("async") {
    uint64_t max_thread_num = 10;
    auto thread = std::make_unique<named_thread_pool>("test_thread", max_thread_num);

    SECTION("multi thread add") {
      uint64_t thread_num = MIN(5, max_thread_num);
      uint64_t total_tx_num = 1000;
      std::atomic<uint64_t> token = thread_num;
      std::future<std::vector<std::optional<consensus::abci::response_check_tx>>> res[thread_num];
      uint64_t tx_num_per_thread = total_tx_num / thread_num;
      for (uint64_t t = 0; t < thread_num; t++) {
        uint64_t offset = t * tx_num_per_thread;
        res[t] = async_thread_pool(thread->get_executor(), [&token, &push_tx, tx_num_per_thread, offset]() {
          token.fetch_sub(1, std::memory_order_seq_cst);
          while (token.load(std::memory_order_seq_cst)) {
          } // wait other thread
          return push_tx(tx_num_per_thread, offset, false);
        });
      }

      for (uint64_t t = 0; t < thread_num; t++) {
        uint64_t added_tx = 0;
        auto result = res[t].get();
        for (auto& r : result) {
          CHECKED_IF(r.has_value()) {
            CHECKED_IF(r.value().result.get()) {
              added_tx++;
            }
          }
        }
        CHECK(added_tx == tx_num_per_thread);
      }

      CHECK(tp.size() == total_tx_num);
    }

    SECTION("1 thread add / 1 thread get") {
      std::atomic<uint64_t> token = 2;

      auto push_res = async_thread_pool(thread->get_executor(), [&push_tx, &token]() {
        token.fetch_sub(1, std::memory_order_seq_cst);
        while (token.load(std::memory_order_seq_cst)) {
        } // wait other thread
        return push_tx(1000, 0, false);
      });

      auto pop_res = async_thread_pool(thread->get_executor(), [&pop_tx, &token]() {
        token.fetch_sub(1, std::memory_order_seq_cst);
        while (token.load(std::memory_order_seq_cst)) {
        } // wait other thread

        uint64_t get_count = 0;
        while (get_count < 1000) {
          auto res = pop_tx(1000 - get_count);
          get_count += res.size();
        }
        return get_count;
      });

      push_res.wait();
      auto res = push_res.get();
      for (auto& r : res) {
        CHECKED_IF(r.has_value()) {
          CHECK(r.value().result.get());
        }
      }

      CHECK(pop_res.get() == 1000);
      CHECK(tp.size() == 0);
    }

    SECTION("1 thread add / 2 thread get") {
      std::atomic<uint64_t> token = 3;
      auto push_res = async_thread_pool(thread->get_executor(), [&push_tx, &token]() {
        token.fetch_sub(1, std::memory_order_seq_cst);
        while (token.load(std::memory_order_seq_cst)) {
        } // wait other thread
        return push_tx(1000, 0, false);
      });

      auto pop_res1 = async_thread_pool(thread->get_executor(), [&pop_tx, &token]() {
        token.fetch_sub(1, std::memory_order_seq_cst);
        while (token.load(std::memory_order_seq_cst)) {
        } // wait other thread

        uint64_t get_count = 0;
        while (get_count < 500) {
          auto res = pop_tx(500 - get_count);
          get_count += res.size();
        }
        return get_count;
      });

      auto pop_res2 = async_thread_pool(thread->get_executor(), [&pop_tx, &token]() {
        token.fetch_sub(1, std::memory_order_seq_cst);
        while (token.load(std::memory_order_seq_cst)) {
        } // wait other thread

        uint64_t get_count = 0;
        while (get_count < 500) {
          auto res = pop_tx(500 - get_count);
          get_count += res.size();
        }
        return get_count;
      });

      push_res.wait();
      auto res = push_res.get();
      for (auto& r : res) {
        CHECKED_IF(r.has_value()) {
          CHECK(r.value().result.get());
        }
      }

      uint64_t get_count = 0;
      get_count += pop_res1.get();
      get_count += pop_res2.get();
      CHECK(get_count == 1000);
      CHECK(tp.size() == 0);
    }
  }
}
