// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash/sha2.h>
#include <noir/mempool/cache.h>

namespace noir::mempool {

void LRUTxCache::reset() {
  std::unique_lock g{mtx};
  tx_keys.clear();
}

bool LRUTxCache::push(const consensus::tx& tx) {
  std::unique_lock g{mtx};

  auto key = crypto::Sha256()(tx);
  auto& list = tx_keys.get<0>();
  auto& cache_map = tx_keys.get<1>();

  if (auto moved = cache_map.find(key); moved != cache_map.end()) {
    list.splice(list.end(), list, tx_keys.project<0>(moved));
    return false;
  }

  if (list.size() >= size) {
    list.pop_front();
  }

  list.push_back(key);

  return true;
}

void LRUTxCache::remove(const consensus::tx& tx) {
  std::unique_lock g{mtx};

  auto key = crypto::Sha256()(tx);
  tx_keys.get<1>().erase(key);
}

} // namespace noir::mempool
