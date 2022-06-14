// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>

#include <noir/codec/datastream.h>
#include <noir/crypto/rand.h>
#include <noir/mempool/cache.h>

using namespace noir;
using namespace noir::mempool;

TEST_CASE("CacheBenchmarks", "[noir][mempool]") {
  auto N = 100;

  BENCHMARK_ADVANCED("CacheInsertTime")(Catch::Benchmark::Chronometer meter) {
    auto cache = LRUTxCache(N);

    auto txs = std::vector<Bytes>(N);
    for (uint64_t i = 0; i < N; i++) {
      txs[i].resize(8);
      codec::BasicDatastream<unsigned char> ds(txs[i]);
      ds.reverse_write((unsigned char*)&i, 8);
    }

    meter.measure([&]() {
      for (auto i = 0; i < N; i++) {
        cache.push(txs[i]);
      }
    });
  };

  BENCHMARK_ADVANCED("CacheRemoveTime")(Catch::Benchmark::Chronometer meter) {
    auto cache = LRUTxCache(N);

    auto txs = std::vector<Bytes>(N);
    for (uint64_t i = 0; i < N; i++) {
      txs[i].resize(8);
      codec::BasicDatastream<unsigned char> ds(txs[i]);
      ds.reverse_write((unsigned char*)&i, 8);
      cache.push(txs[i]);
    }

    meter.measure([&]() {
      for (auto i = 0; i < N; i++) {
        cache.remove(txs[i]);
      }
    });
  };
}
