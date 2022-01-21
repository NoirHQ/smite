// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/hex.h>
#include <noir/tx_pool/unapplied_tx_queue.hpp>
#include <catch2/catch_all.hpp>
#include <catch2/benchmark/catch_benchmark_all.hpp>

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
