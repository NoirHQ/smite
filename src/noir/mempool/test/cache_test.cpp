// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>

#include <noir/crypto/rand.h>
#include <noir/mempool/cache.h>

using namespace noir;
using namespace noir::mempool;

TEST_CASE("CacheRemove", "[noir][mempool]") {
  auto cache = LRUTxCache(100);
  auto num_txs = 10;

  auto txs = std::vector<Bytes>(num_txs);
  for (auto i = 0; i < num_txs; i++) {
    // probability of collision is 2**-256
    auto tx_bytes = Bytes(32);
    crypto::rand_bytes(tx_bytes);
    // TODO: error check for rand_bytes

    txs[i] = tx_bytes;
    cache.push(tx_bytes);

    CHECK((i + 1) == cache.tx_keys.size());
  }

  for (auto i = 0; i < num_txs; i++) {
    cache.remove(txs[i]);
    CHECK((num_txs - (i + 1)) == cache.tx_keys.size());
  }
}
