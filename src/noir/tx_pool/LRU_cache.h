// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types.h>
#include <list>
#include <shared_mutex>

namespace noir::tx_pool {

template<typename K, typename T>
class LRU_cache {
public:
  LRU_cache() = default;

  explicit LRU_cache(uint64_t size): cache_size_(size) {}

  size_t size() {
    std::shared_lock lock(mutex_);
    return key_list_.size();
  }

  bool has(const K& key) {
    std::shared_lock lock(mutex_);
    return key_item_map_.find(key) != key_item_map_.end();
  }

  std::optional<T> get(const K& key) {
    std::shared_lock lock(mutex_);
    if (!has(key)) {
      return std::nullopt;
    }
    return key_item_map_[key];
  }

  void reset() {
    std::unique_lock lock(mutex_);
    key_list_.clear();
    key_itr_map_.clear();
    key_item_map_.clear();
  }

  void put(const K& key, const T& item) {
    std::unique_lock lock(mutex_);

    if (key_item_map_.find(key) != key_item_map_.end()) {
      key_list_.erase(key_itr_map_[key]);
    }

    if (key_list_.size() >= cache_size_) {
      key_item_map_.erase(key_list_.back());
      key_itr_map_.erase(key_list_.back());
      key_list_.pop_back();
    }

    key_list_.push_front(key);
    key_itr_map_[key] = key_list_.begin();
    key_item_map_[key] = item;
  }

  void del(const K& key) {
    std::unique_lock lock(mutex_);

    if (key_item_map_.find(key) == key_item_map_.end()) {
      return;
    }

    key_list_.erase(key_itr_map_[key]);
    key_itr_map_.erase(key);
    key_item_map_.erase(key);
  }

private:
  uint64_t cache_size_ = 100000;
  mutable std::shared_mutex mutex_;
  std::list<K> key_list_;
  std::map<K, decltype(key_list_.begin())> key_itr_map_; // to store key list iterator, for fast remove
  std::map<K, T> key_item_map_;
};

} // namespace noir::tx_pool
