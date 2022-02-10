// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once

#include <noir/db/chain_kv.h>

struct kv_values {
  std::vector<std::pair<noir::db::chain_kv::bytes, noir::db::chain_kv::bytes>> values;

  friend bool operator==(const kv_values& a, const kv_values& b) {
    return a.values == b.values;
  }
};

template<typename S>
S& operator<<(S& s, const kv_values& v) {
  s << "{\n";
  for (auto& kv : v.values) {
    s << "    {{";
    bool first = true;
    for (auto b : kv.first) {
      if (!first)
        s << ", ";
      first = false;
      char x[10];
      sprintf(x, "0x%02x", (uint8_t)b);
      s << x;
    }
    s << "}, {";
    first = true;
    for (auto b : kv.second) {
      if (!first)
        s << ", ";
      first = false;
      char x[10];
      sprintf(x, "0x%02x", (uint8_t)b);
      s << x;
    }
    s << "}},\n";
  }
  s << "}";
  return s;
}

#define KV_REQUIRE_EXCEPTION(expr, msg) CHECK_THROWS(expr, msg);

inline kv_values get_all(noir::db::chain_kv::database& db, const noir::db::chain_kv::bytes& prefix) {
  kv_values result;
  std::unique_ptr<rocksdb::Iterator> rocks_it{db.rdb->NewIterator(rocksdb::ReadOptions())};
  rocks_it->Seek(noir::db::chain_kv::to_slice(prefix));
  while (rocks_it->Valid()) {
    auto k = rocks_it->key();
    if (k.size() < prefix.size() || memcmp(k.data(), prefix.data(), prefix.size()))
      break;
    auto v = rocks_it->value();
    result.values.push_back({noir::db::chain_kv::to_bytes(k), noir::db::chain_kv::to_bytes(v)});
    rocks_it->Next();
  }
  if (!rocks_it->status().IsNotFound())
    noir::db::chain_kv::check(rocks_it->status(), "iterate: ");
  return result;
}

inline kv_values get_values(
  noir::db::chain_kv::write_session& session, const std::vector<noir::db::chain_kv::bytes>& keys) {
  kv_values result;
  for (auto& key : keys) {
    noir::db::chain_kv::bytes value;
    if (auto value = session.get(noir::db::chain_kv::bytes{key}))
      result.values.push_back({key, *value});
  }
  return result;
}

inline kv_values get_matching(
  noir::db::chain_kv::view& view, uint64_t contract, const noir::db::chain_kv::bytes& prefix = {}) {
  kv_values result;
  noir::db::chain_kv::view::iterator it{view, contract, noir::db::chain_kv::to_slice(prefix)};
  ++it;
  while (!it.is_end()) {
    auto kv = it.get_kv();
    if (!kv)
      throw noir::db::chain_kv::exception("iterator read failure");
    result.values.push_back({noir::db::chain_kv::to_bytes(kv->key), noir::db::chain_kv::to_bytes(kv->value)});
    ++it;
  }
  return result;
}

inline kv_values get_matching2(
  noir::db::chain_kv::view& view, uint64_t contract, const noir::db::chain_kv::bytes& prefix = {}) {
  kv_values result;
  noir::db::chain_kv::view::iterator it{view, contract, noir::db::chain_kv::to_slice(prefix)};
  --it;
  while (!it.is_end()) {
    auto kv = it.get_kv();
    if (!kv)
      throw noir::db::chain_kv::exception("iterator read failure");
    result.values.push_back({noir::db::chain_kv::to_bytes(kv->key), noir::db::chain_kv::to_bytes(kv->value)});
    --it;
  }
  std::reverse(result.values.begin(), result.values.end());
  return result;
}

inline kv_values get_it(const noir::db::chain_kv::view::iterator& it) {
  kv_values result;
  auto kv = it.get_kv();
  if (!kv)
    throw noir::db::chain_kv::exception("iterator read failure");
  result.values.push_back({noir::db::chain_kv::to_bytes(kv->key), noir::db::chain_kv::to_bytes(kv->value)});
  return result;
}
