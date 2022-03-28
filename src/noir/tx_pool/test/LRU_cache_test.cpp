// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/tx_pool/LRU_cache.h>
#include <noir/tx_pool/test/tx_pool_test_helper.h>

using namespace noir;
using namespace noir::consensus;
using namespace noir::tx_pool;
using namespace test_detail;

TEST_CASE("LRU_cache: Cache basic test", "[noir][tx_pool]") {
  auto test_helper = std::make_unique<::test_helper>();
  uint tx_count = 1000;
  uint cache_size = 1000;

  LRU_cache<tx_hash, consensus::tx> c{cache_size};

  struct test_tx {
    consensus::tx tx;
    tx_hash hash;
  };
  std::vector<test_tx> txs;
  txs.reserve(tx_count);

  for (uint64_t i = 0; i < tx_count; i++) {
    test_tx test_tx;
    test_tx.tx = test_helper->new_tx();
    test_tx.hash = get_tx_hash(test_tx.tx);
    c.put(test_tx.hash, test_tx.tx);
    txs.push_back(test_tx);
  }

  SECTION("put") {
    CHECK(c.size() == tx_count);
    for (auto& tx : txs) {
      CHECK(c.has(tx.hash));
    }

    // new tx, replace the oldest tx in cache
    auto tx = test_helper->new_tx();
    c.put(get_tx_hash(tx), tx); // tx0 is replaced by new one
    CHECK(c.size() == tx_count);
    CHECK(c.has(get_tx_hash(tx)));
    CHECK(!c.has(txs[0].hash));

    // put again tx1
    c.put(txs[1].hash, txs[1].tx);
    tx = test_helper->new_tx();
    c.put(get_tx_hash(tx), tx); // tx2 is replaced by new one
    CHECK(c.has(get_tx_hash(tx)));
    CHECK(c.has(txs[1].hash));
    CHECK(!c.has(txs[2].hash));
  }

  SECTION("invalid") {
    auto wrapped_tx = test_helper->make_random_wrapped_tx("user");
    auto item = c.get(wrapped_tx->hash());
    CHECK(!item.has_value());
  }

  SECTION("del") {
    CHECK(c.has(txs[3].hash));
    c.del(txs[3].hash);
    CHECK(!c.has(txs[3].hash));
    CHECK(c.size() == tx_count - 1);
  }

  SECTION("get") {
    for (auto& tx : txs) {
      auto res = c.get(tx.hash);
      CHECKED_IF(res.has_value()) {
        CHECK(to_string(tx.hash) == to_string(get_tx_hash(res.value())));
      }
    }
  }
}
