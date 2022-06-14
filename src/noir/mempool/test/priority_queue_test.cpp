// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>

#include <noir/crypto/rand.h>
#include <noir/mempool/priority_queue.h>
#include <boost/asio/io_context.hpp>
#include <algorithm>
#include <random>
#include <thread>

using namespace noir;
using namespace noir::mempool;

TEST_CASE("TxPriorityQueue", "[noir][mempool]") {
  SECTION("Basic") {
    auto pq = TxPriorityQueue();
    auto num_txs = 1000;

    boost::asio::io_context io_context;

    auto priorities = std::vector<int>(num_txs);
    for (auto i = 1; i <= num_txs; i++) {
      priorities[i - 1] = i;

      boost::asio::post(io_context, [&pq, i]() {
        pq.push_tx(std::make_shared<WrappedTx>(WrappedTx{
          .priority = i,
          .timestamp = std::chrono::system_clock().now().time_since_epoch().count(),
        }));
      });
    }

    std::sort(priorities.begin(), priorities.end(), std::greater<>());

    auto th = std::thread([&]() {
      io_context.run();
    });
    io_context.run();
    th.join();

    CHECK(num_txs == pq.num_txs());

    std::this_thread::sleep_for(std::chrono::seconds(1));
    auto now = std::chrono::system_clock().now().time_since_epoch().count();
    pq.push_tx(std::make_shared<WrappedTx>(WrappedTx{
      .priority = 1000,
      .timestamp = now,
    }));
    CHECK(1001 == pq.num_txs());

    auto tx = pq.pop_tx();
    CHECK(1000 == pq.num_txs());
    CHECK(1000 == tx->priority);
    CHECK(now != tx->timestamp);

    auto got_priorities = std::vector<int>();
    while (pq.num_txs() > 0) {
      got_priorities.push_back(pq.pop_tx()->priority);
    }

    CHECK(priorities == got_priorities);
  }

  SECTION("GetEvictableTxs") {
    auto pq = TxPriorityQueue();
    auto gen = std::mt19937(std::chrono::system_clock::now().time_since_epoch().count());
    auto rng = std::uniform_int_distribution<>(0, 99999);

    auto values = std::vector<int>(1000);

    for (auto i = 0; i < 1000; i++) {
      auto tx = Bytes(5);
      CHECK(crypto::rand_bytes(tx));

      auto x = rng(gen);
      pq.push_tx(std::make_shared<WrappedTx>(WrappedTx{
        .tx = tx,
        .priority = x,
      }));

      values[i] = x;
    }

    std::sort(values.begin(), values.end());

    auto max = *values.rbegin();
    auto min = *values.begin();
    auto total_size = int64_t(values.size() * 5);

    struct TestCase {
      std::string name;
      int64_t priority;
      int64_t tx_size;
      int64_t total_size;
      int64_t cap;
      int64_t expected_len;
    };

    auto test_cases = std::vector<TestCase>{
      {
        .name = "largest priority; single tx",
        .priority = int64_t(max + 1),
        .tx_size = 5,
        .total_size = total_size,
        .cap = total_size,
        .expected_len = 1,
      },
      {
        .name = "largest priority; multi tx",
        .priority = int64_t(max + 1),
        .tx_size = 17,
        .total_size = total_size,
        .cap = total_size,
        .expected_len = 4,
      },
      {
        .name = "largest priority; out of capacity",
        .priority = int64_t(max + 1),
        .tx_size = total_size + 1,
        .total_size = total_size,
        .cap = total_size,
        .expected_len = 0,
      },
      {
        .name = "smallest priority; no tx",
        .priority = int64_t(min - 1),
        .tx_size = 5,
        .total_size = total_size,
        .cap = total_size,
        .expected_len = 0,
      },
      {
        .name = "small priority; no tx",
        .priority = int64_t(min),
        .tx_size = 5,
        .total_size = total_size,
        .cap = total_size,
        .expected_len = 0,
      },
    };

    for (auto& tc : test_cases) {
      auto evic_txs = pq.get_evictable_txs(tc.priority, tc.tx_size, tc.total_size, tc.cap);
      CHECK(evic_txs.size() == tc.expected_len);
    }
  }

  SECTION("RemoveTx") {
    auto pq = TxPriorityQueue();
    auto gen = std::mt19937(std::chrono::system_clock::now().time_since_epoch().count());
    auto num_txs = 1000;

    auto values = std::vector<int>(num_txs);

    auto rng = std::uniform_int_distribution<>(0, 999999);
    for (auto i = 0; i < num_txs; i++) {
      auto x = rng(gen);
      pq.push_tx(std::make_shared<WrappedTx>(WrappedTx{
        .priority = x,
      }));

      values[i] = x;
    }

    CHECK(num_txs == pq.num_txs());

    std::sort(values.begin(), values.end());
    auto max = *values.rbegin();

    // not accessible to the position with ordered_non_unqiue index
    auto i = pq.num_txs() / 2;
    auto it = pq.txs.get<1>().begin();
    while (i-- > 0) {
      it++;
    }
    pq.remove_tx(*it);
    CHECK((num_txs - 1) == pq.num_txs());
    CHECK(max == pq.pop_tx()->priority);
    CHECK((num_txs - 2) == pq.num_txs());

    // XXX: remove_tx with heap_index out of range is unsupported
    // pq.remove_tx(...);
  }
}
