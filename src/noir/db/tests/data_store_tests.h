// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once

#include <catch2/catch_all.hpp>
#include <random>
#include <string>
#include <type_traits>
#include <unordered_map>

#include <rocksdb/db.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/options.h>
#include <rocksdb/slice_transform.h>

#include <noir/db/rocks_session.h>
#include <noir/db/session.h>

namespace noir::db::session_tests {

inline std::shared_ptr<rocksdb::DB> make_rocks_db(const std::string& name = "/tmp/testdb") {
  rocksdb::DestroyDB(name.c_str(), rocksdb::Options{});

  rocksdb::DB* cache_ptr{nullptr};
  auto cache = std::shared_ptr<rocksdb::DB>{};

  auto options = rocksdb::Options{};
  options.create_if_missing = true;
  options.level_compaction_dynamic_level_bytes = true;
  options.bytes_per_sync = 1048576;
  options.OptimizeLevelStyleCompaction(256ull << 20);

  auto status = rocksdb::DB::Open(options, name.c_str(), &cache_ptr);
  cache.reset(cache_ptr);

  return cache;
}

static const std::unordered_map<std::string, std::string> char_key_values{
  {"a", "123456789"},
  {"b", "abcdefghi"},
  {"c", "987654321"},
  {"d", "ABCDEFGHI"},
  {"e", "HELLO WORLD"},
  {"f", "Hello World"},
  {"aa", "Foo"},
  {"bb", "Bar"},
  {"cc", "FooBar"},
  {"dd", "Fizz"},
  {"ee", "Buzz"},
  {"ff", "FizzBuzz"},
  {"aaa", "qwerty"},
  {"bbb", "QWERTY"},
  {"ccc", "10101010101010"},
  {"ddd", "00000000000000"},
  {"eee", "01010101010101"},
  {"fff", "11111111111111"},
  {"aaaaa", "000000001111111"},
  {"bbbbb", "111111110000000"},
  {"ccccc", "1"},
  {"ddddd", "2"},
  {"eeeee", "3"},
  {"fffff", "5"},
  {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
  {"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
  {"cccccccccccccccccccccccccccccccccccccccccccccccccccccc", "dddddddddddddddddddddddddddddddddddddddddddddddddddddd"},
  {"dddddddddddddddddddddddddddddddddddddddddddddddddddddd", "cccccccccccccccccccccccccccccccccccccccccccccccccccccc"},
  {"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee", "ffffffffffffffffffffffffffffffffffffffffffffffffffffff"},
  {"ffffffffffffffffffffffffffffffffffffffffffffffffffffff", "ffffffffffffffffffffffffffffffffffffffffffffffffffffff"},
};

static const std::vector<std::pair<noir::db::session::shared_bytes, noir::db::session::shared_bytes>> char_batch_values{
  {noir::db::session::shared_bytes("hello0", 6), noir::db::session::shared_bytes("world0", 6)},
  {noir::db::session::shared_bytes("hello1", 6), noir::db::session::shared_bytes("world1", 6)},
  {noir::db::session::shared_bytes("hello2", 6), noir::db::session::shared_bytes("world2", 6)},
  {noir::db::session::shared_bytes("hello3", 6), noir::db::session::shared_bytes("world3", 6)},
  {noir::db::session::shared_bytes("hello4", 6), noir::db::session::shared_bytes("world4", 6)},
  {noir::db::session::shared_bytes("hello5", 6), noir::db::session::shared_bytes("world5", 6)},
  {noir::db::session::shared_bytes("hello6", 6), noir::db::session::shared_bytes("world6", 6)},
  {noir::db::session::shared_bytes("hello7", 6), noir::db::session::shared_bytes("world7", 6)},
  {noir::db::session::shared_bytes("hello8", 6), noir::db::session::shared_bytes("world8", 6)},
  {noir::db::session::shared_bytes("hello9", 6), noir::db::session::shared_bytes("world9", 6)},
};

static const std::unordered_map<int32_t, int32_t> int_key_values{
  {1, 1},
  {3, 2},
  {5, 3},
  {7, 4},
  {9, 5},
  {11, 6},
  {13, 7},
  {15, 8},
  {14, 9},
  {12, 10},
  {10, 11},
  {8, 12},
  {6, 13},
  {4, 14},
  {2, 15},
};

static const std::vector<int32_t> int_keys{
  100,
  200,
  300,
  400,
  500,
  600,
  700,
  800,
  900,
  1000,
};

static const std::vector<int32_t> int_values{
  1000,
  2000,
  3000,
  4000,
  5000,
  6000,
  7000,
  8000,
  9000,
  10000,
};

static const std::vector<std::pair<noir::db::session::shared_bytes, noir::db::session::shared_bytes>> int_batch_values{
  {noir::db::session::shared_bytes(&int_keys[0], 1), noir::db::session::shared_bytes(&int_values[0], 1)},
  {noir::db::session::shared_bytes(&int_keys[1], 1), noir::db::session::shared_bytes(&int_values[1], 1)},
  {noir::db::session::shared_bytes(&int_keys[2], 1), noir::db::session::shared_bytes(&int_values[2], 1)},
  {noir::db::session::shared_bytes(&int_keys[3], 1), noir::db::session::shared_bytes(&int_values[3], 1)},
  {noir::db::session::shared_bytes(&int_keys[4], 1), noir::db::session::shared_bytes(&int_values[4], 1)},
  {noir::db::session::shared_bytes(&int_keys[5], 1), noir::db::session::shared_bytes(&int_values[5], 1)},
  {noir::db::session::shared_bytes(&int_keys[6], 1), noir::db::session::shared_bytes(&int_values[6], 1)},
  {noir::db::session::shared_bytes(&int_keys[7], 1), noir::db::session::shared_bytes(&int_values[7], 1)},
  {noir::db::session::shared_bytes(&int_keys[8], 1), noir::db::session::shared_bytes(&int_values[8], 1)},
  {noir::db::session::shared_bytes(&int_keys[9], 1), noir::db::session::shared_bytes(&int_values[9], 1)},
};

struct string_t {};
struct int_t {};

template<typename T, typename Key, typename Value>
void make_data_store(T& ds, const std::unordered_map<Key, Value>& kvs, string_t) {
  for (const auto& kv : kvs) {
    ds.write(noir::db::session::shared_bytes(kv.first.c_str(), kv.first.size()),
      noir::db::session::shared_bytes(kv.second.c_str(), kv.second.size()));
  }
}

template<typename T, typename Key, typename Value>
void make_data_store(T& ds, const std::unordered_map<Key, Value>& kvs, int_t) {
  for (const auto& kv : kvs) {
    ds.write(noir::db::session::shared_bytes(&kv.first, 1), noir::db::session::shared_bytes(&kv.second, 1));
  }
}

template<typename T, typename Key, typename Value>
void verify_equal(T& ds, const std::unordered_map<Key, Value>& container, string_t) {
  auto verify_key_value = [&](auto kv) {
    auto key = std::string{std::begin(kv.first), std::end(kv.first)};
    auto it = container.find(key);
    CHECK(it != std::end(container));
    auto buffer =
      std::vector<noir::db::session::shared_bytes::underlying_type_t>{std::begin(*kv.second), std::end(*kv.second)};
    CHECK(std::memcmp(it->second.c_str(), buffer.data(), it->second.size()) == 0);
  };

  for (const auto& kv : ds) {
    verify_key_value(kv);
  }

  auto begin = std::begin(ds);
  auto end = std::end(ds);
  auto current = end;
  --current;
  auto count = size_t{0};
  while (true) {
    verify_key_value(*current);
    ++count;
    if (current == begin) {
      break;
    }
    --current;
  }
  CHECK(count == container.size());

  for (const auto& it : container) {
    auto key = noir::db::session::shared_bytes(it.first.c_str(), it.first.size());
    auto value = ds.read(key);
    CHECK(ds.contains(key) == true);
    CHECK(value.has_value());
    auto buffer = std::vector<noir::db::session::shared_bytes::underlying_type_t>{std::begin(*value), std::end(*value)};
    CHECK(std::memcmp(it.second.c_str(), buffer.data(), it.second.size()) == 0);
  }
}

template<typename Data_store, typename Key, typename Value>
void verify_equal(
  noir::db::session::session<Data_store>& ds, const std::unordered_map<Key, Value>& container, string_t) {
  auto verify_key_value = [&](auto kv) {
    auto key = std::string{std::begin(kv.first), std::end(kv.first)};
    auto it = container.find(key);
    CHECK(it != std::end(container));
    auto buffer =
      std::vector<noir::db::session::shared_bytes::underlying_type_t>{std::begin(*kv.second), std::end(*kv.second)};
    CHECK(std::memcmp(it->second.c_str(), buffer.data(), it->second.size()) == 0);
  };

  // the iterator is a session is circular.  So we need to bail out when we circle around to the beginning.
  auto begin = std::begin(ds);
  auto kv_it = std::begin(ds);
  auto count = size_t{0};
  do {
    if (kv_it != std::end(ds)) {
      verify_key_value(*kv_it);
      ++count;
    }
    ++kv_it;
  } while (kv_it != begin);
  CHECK(count == container.size());

  kv_it = std::end(ds);
  --kv_it;
  count = 0;
  while (true) {
    verify_key_value(*kv_it);
    ++count;
    if (kv_it == begin) {
      break;
    }
    --kv_it;
  }
  CHECK(count == container.size());

  for (const auto& it : container) {
    auto key = noir::db::session::shared_bytes(it.first.c_str(), it.first.size());
    auto value = ds.read(key);
    CHECK(ds.contains(key) == true);
    CHECK(value.has_value());
    auto buffer = std::vector<noir::db::session::shared_bytes::underlying_type_t>{std::begin(*value), std::end(*value)};
    CHECK(std::memcmp(it.second.c_str(), buffer.data(), it.second.size()) == 0);
  }
}

template<typename T, typename Key, typename Value>
void verify_equal(T& ds, const std::unordered_map<Key, Value>& container, int_t) {
  auto verify_key_value = [&](auto kv) {
    auto buffer =
      std::vector<noir::db::session::shared_bytes::underlying_type_t>{std::begin(kv.first), std::end(kv.first)};
    auto it = container.find(*reinterpret_cast<const Key*>(buffer.data()));
    CHECK(it != std::end(container));
    buffer =
      std::vector<noir::db::session::shared_bytes::underlying_type_t>{std::begin(*kv.second), std::end(*kv.second)};
    CHECK(std::memcmp(reinterpret_cast<const void*>(&it->second), buffer.data(), sizeof(Value)) == 0);
  };

  for (const auto& kv : ds) {
    verify_key_value(kv);
  }

  auto begin = std::begin(ds);
  auto end = std::end(ds);
  auto current = end;
  --current;
  auto count = size_t{0};
  while (true) {
    verify_key_value(*current);
    ++count;
    if (current == begin) {
      break;
    }
    --current;
  }
  CHECK(count == container.size());

  for (const auto& it : container) {
    auto key = noir::db::session::shared_bytes(&it.first, 1);
    auto value = ds.read(key);
    CHECK(value.has_value());
    CHECK(ds.contains(key) == true);
    auto buffer = std::vector<noir::db::session::shared_bytes::underlying_type_t>{std::begin(*value), std::end(*value)};
    CHECK(std::memcmp(reinterpret_cast<const void*>(&it.second), buffer.data(), sizeof(Value)) == 0);
  }
}

template<typename Data_store, typename Key, typename Value>
void verify_equal(noir::db::session::session<Data_store>& ds, const std::unordered_map<Key, Value>& container, int_t) {
  auto verify_key_value = [&](auto kv) {
    auto buffer =
      std::vector<noir::db::session::shared_bytes::underlying_type_t>{std::begin(kv.first), std::end(kv.first)};
    auto it = container.find(*reinterpret_cast<const Key*>(buffer.data()));
    CHECK(it != std::end(container));
    buffer =
      std::vector<noir::db::session::shared_bytes::underlying_type_t>{std::begin(*kv.second), std::end(*kv.second)};
    CHECK(std::memcmp(reinterpret_cast<const void*>(&it->second), buffer.data(), sizeof(Value)) == 0);
  };

  // the iterator is a session is circular.  So we need to bail out when we circle around to the beginning.
  auto begin = std::begin(ds);
  auto kv_it = std::begin(ds);
  auto count = size_t{0};
  do {
    if (kv_it != std::end(ds)) {
      verify_key_value(*kv_it);
      ++count;
    }
    ++kv_it;
  } while (kv_it != begin);
  CHECK(count == container.size());

  kv_it = std::end(ds);
  --kv_it;
  count = 0;
  while (true) {
    verify_key_value(*kv_it);
    ++count;
    if (kv_it == begin) {
      break;
    }
    --kv_it;
  }
  CHECK(count == container.size());

  for (const auto& it : container) {
    auto key = noir::db::session::shared_bytes(&it.first, 1);
    auto value = ds.read(key);
    CHECK(value.has_value());
    CHECK(ds.contains(key) == true);
    auto buffer = std::vector<noir::db::session::shared_bytes::underlying_type_t>{std::begin(*value), std::end(*value)};
    CHECK(std::memcmp(reinterpret_cast<const void*>(&it.second), buffer.data(), sizeof(Value)) == 0);
  }
}

template<typename T>
void verify_iterators(T& ds, string_t) {
  CHECK(ds.find(noir::db::session::shared_bytes("g", 1)) == std::end(ds));
  CHECK(ds.find(noir::db::session::shared_bytes("a", 1)) != std::end(ds));
  CHECK(*ds.find(noir::db::session::shared_bytes("a", 1)) ==
    std::pair(noir::db::session::shared_bytes("a", 1),
      std::optional<noir::db::session::shared_bytes>{noir::db::session::shared_bytes("123456789", 9)}));
  CHECK(*std::begin(ds) ==
    std::pair(noir::db::session::shared_bytes("a", 1),
      std::optional<noir::db::session::shared_bytes>{noir::db::session::shared_bytes("123456789", 9)}));
  CHECK(std::begin(ds) != std::end(ds));
  CHECK(*ds.lower_bound(noir::db::session::shared_bytes("fffff", 5)) ==
    std::pair(noir::db::session::shared_bytes("fffff", 5),
      std::optional<noir::db::session::shared_bytes>{noir::db::session::shared_bytes("5", 1)}));
}

template<typename T>
void verify_iterators(T& ds, int_t) {
  auto search_key = int32_t{16};
  CHECK(ds.find(noir::db::session::shared_bytes(&search_key, 1)) == std::end(ds));
  search_key = 15;
  auto search_value = 8;
  CHECK(ds.find(noir::db::session::shared_bytes(&search_key, 1)) != std::end(ds));
  CHECK(*ds.find(noir::db::session::shared_bytes(&search_key, 1)) ==
    std::pair(noir::db::session::shared_bytes(&search_key, 1),
      std::optional<noir::db::session::shared_bytes>{noir::db::session::shared_bytes(&search_value, 1)}));
  search_key = 1;
  search_value = 1;
  CHECK(*std::begin(ds) ==
    std::pair(noir::db::session::shared_bytes(&search_key, 1),
      std::optional<noir::db::session::shared_bytes>{noir::db::session::shared_bytes(&search_value, 1)}));
  CHECK(std::begin(ds) != std::end(ds));
  search_key = 14;
  search_value = 9;
  auto result_key = int32_t{14};
  auto result_value = int32_t{9};
  CHECK(*ds.lower_bound(noir::db::session::shared_bytes(&search_key, 1)) ==
    std::pair(noir::db::session::shared_bytes(&result_key, 1),
      std::optional<noir::db::session::shared_bytes>{noir::db::session::shared_bytes(&result_value, 1)}));
}

template<typename T>
void verify_key_order(T& ds) {
  auto begin_key = noir::db::session::shared_bytes{};
  auto current_key = noir::db::session::shared_bytes{};
  auto compare = std::less<noir::db::session::shared_bytes>{};
  for (const auto& kv : ds) {
    if (!current_key) {
      current_key = kv.first;
      begin_key = kv.first;
      continue;
    }

    if (current_key == begin_key) {
      // We've wrapped around
      break;
    }

    CHECK(compare(current_key, kv.first) == true);
    current_key = kv.first;
  }
}

template<typename T>
void verify_session_key_order(T& ds) {
  auto current_key = noir::db::session::shared_bytes{};
  auto compare = std::less<noir::db::session::shared_bytes>{};

  // the iterator is a session is circular.  So we need to bail out when we circle around to the beginning.
  auto begin = std::begin(ds);
  auto kv_it = std::begin(ds);
  do {
    if (kv_it == std::end(ds)) {
      ++kv_it;
      continue;
    }

    auto kv = *kv_it;
    if (!current_key) {
      current_key = kv.first;
      ++kv_it;
      continue;
    }

    CHECK(compare(current_key, kv.first) == true);
    current_key = kv.first;
    ++kv_it;
  } while (kv_it != begin);
}

template<typename T>
void verify_rwd(T& ds, const noir::db::session::shared_bytes& key, const noir::db::session::shared_bytes& value) {
  CHECK(!ds.read(key).has_value());
  CHECK(ds.contains(key) == false);

  ds.write(key, value);
  CHECK(ds.read(key) == value);
  CHECK(ds.contains(key) == true);

  ds.erase(key);
  CHECK(!ds.read(key).has_value());
  CHECK(ds.contains(key) == false);
}

template<typename T, typename Iterable>
void verify_rwd_batch(T& ds, const Iterable& kvs) {
  auto keys = std::vector<noir::db::session::shared_bytes>{};
  for (const auto& kv : kvs) {
    keys.emplace_back(kv.first);
  }

  auto [read_batch1, not_found1] = ds.read(keys);
  CHECK(read_batch1.empty() == true);
  for (const auto& kv : kvs) {
    CHECK(!ds.read(kv.first).has_value());
    CHECK(ds.contains(kv.first) == false);
    CHECK(not_found1.find(kv.first) != std::end(not_found1));
  }

  ds.write(kvs);
  auto [read_batch2, not_found2] = ds.read(keys);
  CHECK(read_batch2.empty() == false);
  for (const auto& kv : kvs) {
    CHECK(ds.read(kv.first).has_value());
    CHECK(ds.contains(kv.first) == true);
    CHECK(not_found2.find(kv.first) == std::end(not_found2));
  }

  ds.erase(keys);
  auto [read_batch3, not_found3] = ds.read(keys);
  CHECK(read_batch3.empty() == true);
  for (const auto& kv : kvs) {
    CHECK(!ds.read(kv.first).has_value());
    CHECK(ds.contains(kv.first) == false);
    CHECK(not_found3.find(kv.first) != std::end(not_found3));
  }
}

template<typename T, typename U>
void verify_read_from_datastore(T& ds, U& other_ds) {
  auto compare_ds = [](auto& left, auto& right) {
    // The data stores are equal if all the key_values in left are in right
    // and all the key_values in right are in left.
    for (const auto& kv : left) {
      CHECK(right.contains(kv.first) == true);
      CHECK(right.read(kv.first) == kv.second);
    }

    for (const auto& kv : right) {
      CHECK(left.contains(kv.first) == true);
      CHECK(left.read(kv.first) == kv.second);
    }
  };

  auto keys = std::vector<noir::db::session::shared_bytes>{};
  for (const auto& kv : ds) {
    keys.emplace_back(kv.first);
  }

  other_ds.read_from(ds, keys);
  compare_ds(other_ds, ds);
}

template<typename Data_store>
void verify_read_from_datastore(
  noir::db::session::session<Data_store>& ds, noir::db::session::session<Data_store>& other_ds) {
  auto compare_ds = [](auto& left, auto& right) {
    // The data stores are equal if all the key_values in left are in right
    // and all the key_values in right are in left.
    // the iterator is a session is circular.  So we need to bail out when we circle around to the beginning.
    auto begin1 = std::begin(left);
    auto kv_it1 = std::begin(left);
    do {
      if (kv_it1 != std::end(left)) {
        auto kv = *kv_it1;
        CHECK(right.contains(kv.first) == true);
        CHECK(right.read(kv.first) == kv.second);
      }
      ++kv_it1;
    } while (kv_it1 != begin1);

    auto begin2 = std::begin(right);
    auto kv_it2 = std::begin(right);
    do {
      if (kv_it2 != std::end(right)) {
        auto kv = *kv_it2;
        CHECK(left.contains(kv.first) == true);
        CHECK(left.read(kv.first) == kv.second);
      }
      ++kv_it2;
    } while (kv_it2 != begin2);
  };

  auto keys = std::vector<noir::db::session::shared_bytes>{};
  auto begin = std::begin(ds);
  auto kv_it = std::begin(ds);
  do {
    if (kv_it != std::end(ds)) {
      auto kv = *kv_it;
      keys.emplace_back(kv.first);
    }
    ++kv_it;
  } while (kv_it != begin);

  other_ds.read_from(ds, keys);
  compare_ds(other_ds, ds);
}

template<typename T, typename U>
void verify_write_to_datastore(T& ds, U& other_ds) {
  auto compare_ds = [](auto& left, auto& right) {
    // The data stores are equal if all the key_values in left are in right
    // and all the key_values in right are in left.
    for (const auto& kv : left) {
      CHECK(right.contains(kv.first) == true);
      CHECK(right.read(kv.first) == kv.second);
    }

    for (const auto& kv : right) {
      CHECK(left.contains(kv.first) == true);
      CHECK(left.read(kv.first) == kv.second);
    }
  };

  auto keys = std::vector<noir::db::session::shared_bytes>{};
  for (const auto& kv : ds) {
    keys.emplace_back(kv.first);
  }

  ds.write_to(other_ds, keys);
  compare_ds(other_ds, ds);
}

template<typename Data_store>
void verify_write_to_datastore(
  noir::db::session::session<Data_store>& ds, noir::db::session::session<Data_store>& other_ds) {
  auto compare_ds = [](auto& left, auto& right) {
    // The data stores are equal if all the key_values in left are in right
    // and all the key_values in right are in left.
    // the iterator is a session is circular.  So we need to bail out when we circle around to the beginning.
    auto begin1 = std::begin(left);
    auto kv_it1 = std::begin(left);
    do {
      if (kv_it1 != std::end(left)) {
        auto kv = *kv_it1;
        CHECK(right.contains(kv.first) == true);
        CHECK(right.read(kv.first) == kv.second);
      }
      ++kv_it1;
    } while (kv_it1 != begin1);

    auto begin2 = std::begin(right);
    auto kv_it2 = std::begin(right);
    do {
      if (kv_it2 != std::end(right)) {
        auto kv = *kv_it2;
        CHECK(left.contains(kv.first) == true);
        CHECK(left.read(kv.first) == kv.second);
      }
      ++kv_it2;
    } while (kv_it2 != begin2);
  };

  auto keys = std::vector<noir::db::session::shared_bytes>{};
  auto begin = std::begin(ds);
  auto kv_it = std::begin(ds);
  do {
    if (kv_it != std::end(ds)) {
      auto kv = *kv_it;
      keys.emplace_back(kv.first);
    }
    ++kv_it;
  } while (kv_it != begin);

  ds.write_to(other_ds, keys);
  compare_ds(other_ds, ds);
}

inline noir::db::session::session<noir::db::session::rocksdb_t> make_session(const std::string& name = "/tmp/testdb") {
  auto rocksdb = make_rocks_db(name);
  return noir::db::session::make_session(std::move(rocksdb), 16);
}

template<typename Data_store, typename Container>
void verify(const Data_store& ds, const Container& kvs) {
  for (auto kv : ds) {
    auto buffer =
      std::vector<noir::db::session::shared_bytes::underlying_type_t>{std::begin(kv.first), std::end(kv.first)};
    auto current_key = *reinterpret_cast<const uint16_t*>(buffer.data());
    buffer =
      std::vector<noir::db::session::shared_bytes::underlying_type_t>{std::begin(*kv.second), std::end(*kv.second)};
    auto current_value = *reinterpret_cast<const uint16_t*>(buffer.data());

    auto it = kvs.find(current_key);
    CHECK(it != std::end(kvs));
    CHECK(it->first == current_key);
    CHECK(it->second == current_value);
  }

  for (auto kv : kvs) {
    auto key = noir::db::session::shared_bytes(&kv.first, 1);
    auto value = noir::db::session::shared_bytes(&kv.second, 1);
    auto result_value = ds.read(key);
    CHECK(result_value.value());
    CHECK(value == result_value);
  }
};

template<typename Data_store, typename Container>
void write(Data_store& ds, const Container& kvs) {
  for (auto kv : kvs) {
    ds.write(noir::db::session::shared_bytes(&kv.first, 1), noir::db::session::shared_bytes(&kv.second, 1));
  }
};

inline std::unordered_map<uint16_t, uint16_t> generate_kvs(size_t size) {
  std::random_device random_device;
  std::mt19937 generator{random_device()};
  std::uniform_int_distribution<uint16_t> distribution{0, std::numeric_limits<uint16_t>::max()};

  auto container = std::unordered_map<uint16_t, uint16_t>{};
  for (size_t i = 0; i < size; ++i) {
    container.emplace(distribution(generator), distribution(generator));
  }
  return container;
};

inline std::unordered_map<uint16_t, uint16_t> collapse(
  const std::vector<std::unordered_map<uint16_t, uint16_t>>& kvs_list) {
  if (kvs_list.empty()) {
    return std::unordered_map<uint16_t, uint16_t>{};
  }

  auto merged = kvs_list[0];
  for (size_t i = 1; i < kvs_list.size(); ++i) {
    auto& list = kvs_list[i];

    for (auto kv : list) {
      merged.insert_or_assign(kv.first, kv.second);
    }
  }
  return merged;
};

} // namespace noir::db::session_tests
