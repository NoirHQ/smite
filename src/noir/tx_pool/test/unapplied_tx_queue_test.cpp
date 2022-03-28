// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/tx_pool/test/tx_pool_test_helper.h>
#include <noir/tx_pool/unapplied_tx_queue.h>

using namespace noir;
using namespace noir::consensus;
using namespace noir::tx_pool;
using namespace test_detail;

TEST_CASE("unapplied_tx_queue: Tx queue basic test", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  unapplied_tx_queue tx_queue;

  const uint64_t tx_count = 10;
  wrapped_tx_ptrs wtxs;
  wtxs.reserve(tx_count);
  for (auto i = 0; i < tx_count; i++) {
    wtxs.push_back(test_helper->make_random_wrapped_tx("user"));
  }

  // Add tx
  for (auto& wtx : wtxs) {
    CHECK(tx_queue.add_tx(wtx));
  }
  CHECK(tx_queue.size() == tx_count);

  SECTION("Add same tx hash") { // fail case
    auto wtx = wtxs[0];
    wtx->nonce = wtxs[tx_count - 1]->nonce + 1;
    CHECK(tx_queue.add_tx(wtx));
    CHECK(tx_queue.size() == tx_count);
    CHECK(tx_queue.get_tx(wtxs[0]->hash()));
  }

  SECTION("Has") {
    for (auto& wtx : wtxs) {
      CHECK(tx_queue.has(wtx->hash()));
    }
  }

  SECTION("Get") {
    for (auto& wtx : wtxs) {
      CHECK(tx_queue.get_tx(wtx->hash()));
    }
  }

  SECTION("Erase tx") {
    for (auto& wtx : wtxs) {
      CHECK(tx_queue.erase(wtx->hash()));
    }
    CHECK(tx_queue.empty());

    // fail case
    for (auto& wtx : wtxs) {
      CHECK(tx_queue.erase(wtx->hash()) == false);
    }
    CHECK(tx_queue.empty());
  }

  SECTION("Erase tx by iterator") {
    std::for_each(tx_queue.begin(), tx_queue.end(), [&](auto& itr) { CHECK(tx_queue.erase(itr.hash())); });
    CHECK(tx_queue.empty());
  }

  SECTION("Flush") {
    tx_queue.clear();
    CHECK(tx_queue.empty());
  }
}

TEST_CASE("unapplied_tx_queue: Fully add/erase tx", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  uint64_t tx_count = 10000;
  uint64_t queue_size = 0;
  wrapped_tx_ptrs wtxs;
  wtxs.reserve(tx_count);
  for (uint64_t i = 0; i < tx_count; i++) {
    wtxs.push_back(test_helper->make_random_wrapped_tx("user"));
  }

  for (auto& wtx : wtxs) {
    queue_size += unapplied_tx_queue::bytes_size(wtx);
  }

  auto tx_queue = std::make_unique<unapplied_tx_queue>(queue_size);
  for (auto& wtx : wtxs) {
    CHECK(tx_queue->add_tx(wtx));
  }
  CHECK(tx_queue->size() == tx_count);

  for (auto& wtx : wtxs) {
    CHECK(tx_queue->erase(wtx->hash()));
  }
  CHECK(tx_queue->empty());
}

TEST_CASE("unapplied_tx_queue: Indexing", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  uint64_t tx_count = 10000;
  uint64_t user_count = tx_count / 100;
  uint64_t queue_size = config{}.max_tx_bytes * tx_count;
  auto tx_queue = std::make_unique<unapplied_tx_queue>(queue_size);

  for (uint64_t i = 0; i < tx_count; i++) {
    CHECK(tx_queue->add_tx(test_helper->make_random_wrapped_tx("user" + std::to_string(i / user_count))));
  }
  CHECK(tx_queue->size() == tx_count);

  SECTION("by nonce") {
    auto tx = test_helper->make_random_wrapped_tx("Alice");
    tx_queue->add_tx(tx);
    auto tx_ptr = tx_queue->get_tx(tx->sender, tx->nonce);
    CHECKED_IF(tx_ptr) {
      CHECK(tx_ptr->nonce == tx->nonce);
    }
  }

  SECTION("a specific sender's txs") {
    SECTION("all txs") {
      uint64_t tx_count_per_user = tx_count / user_count;
      for (uint64_t i = 0; i < user_count; i++) {
        address_type sender = str_to_addr("user" + std::to_string(i));
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
