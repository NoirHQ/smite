// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//

#include <noir/consensus/db/db.h>

namespace noir::consensus {

bool simple_db::get(const noir::p2p::bytes& key, noir::p2p::bytes& val) const {
  return _impl->get(key, val);
}

bool simple_db::has(const noir::p2p::bytes& key, bool& val) const {
  return _impl->has(key, val);
}

bool simple_db::set(const noir::p2p::bytes& key, const noir::p2p::bytes& val) {
  return _impl->set(key, val);
}

bool simple_db::set_sync(const noir::p2p::bytes& key, const noir::p2p::bytes& val) {
  return _impl->set_sync(key, val);
}

bool simple_db::del(const noir::p2p::bytes& key) {
  return _impl->del(key);
}

bool simple_db::del_sync(const noir::p2p::bytes& key) {
  return _impl->del_sync(key);
}

bool simple_db::close() {
  return _impl->close();
}

bool simple_db::print() const {
  return _impl->print();
}

const std::map<noir::p2p::bytes, noir::p2p::bytes>& simple_db::stats() {
  return _impl->stats();
}

bool simple_db::simple_db_impl::get(const noir::p2p::bytes& key, noir::p2p::bytes& val) const {
  if (auto iter = _map.find(key); iter != _map.end()) {
    noir::p2p::bytes tmp(iter->second.begin(), iter->second.end());
    val = tmp;
    return true;
  }
  return false;
}

bool simple_db::simple_db_impl::has(const noir::p2p::bytes& key, bool& val) const {
  auto iter = _map.find(key);
  val = (iter != _map.end());
  return true;
}

bool simple_db::simple_db_impl::set(const noir::p2p::bytes& key, const noir::p2p::bytes& val) {
  _map.emplace(key, val);
  return true;
}

bool simple_db::simple_db_impl::set_sync(const noir::p2p::bytes& key, const noir::p2p::bytes& val) {
  set(key, val);
  return true;
}

bool simple_db::simple_db_impl::del(const noir::p2p::bytes& key) {
  _map.erase(key);
  return true;
}

bool simple_db::simple_db_impl::del_sync(const noir::p2p::bytes& key) {
  del(key);
  return true;
}

bool simple_db::simple_db_impl::close() {
  return true;
}

bool simple_db::simple_db_impl::print() const {
  return true;
}

const std::map<noir::p2p::bytes, noir::p2p::bytes>& simple_db::simple_db_impl::stats() {
  return _map;
}

bool simple_db::simple_db_batch::set(const noir::p2p::bytes& key, const noir::p2p::bytes& val) {
  if (_is_closed) {
    return false;
  }
  _map.emplace(key, std::make_pair(true, val));
  return true;
}

bool simple_db::simple_db_batch::del(const noir::p2p::bytes& key) {
  if (_is_closed) {
    return false;
  }
  _map.emplace(key, std::make_pair(false, noir::p2p::bytes{}));
  return true;
}

bool simple_db::simple_db_batch::write() {
  if (_is_closed) {
    return false;
  }
  std::for_each(_map.begin(), _map.end(), [&](const auto& t) {
    if (t.second.first == true) {
      _db->set(t.first, t.second.second);
    } else {
      _db->del(t.first);
    }
  });
  return true;
}

bool simple_db::simple_db_batch::write_sync() { return write(); }

bool simple_db::simple_db_batch::close() {
  _is_closed = true;
  return true;
}

} // namespace noir::consensus
