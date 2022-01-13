// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/p2p/types.h>

namespace noir::consensus {

template<typename K, typename V>
class db {
public:
  virtual ~db(){};
  virtual bool get(const K& key, V& val) const = 0;
  virtual bool has(const K& key, bool& val) const = 0;
  virtual bool set(const K& key, const V& val) = 0;
  virtual bool set_sync(const K& key, const V& val) = 0;
  virtual bool del(const K& key) = 0;
  virtual bool del_sync(const K& key) = 0;
  virtual bool close() = 0;
  virtual bool print() const = 0;
  virtual const std::map<K, V>& stats() = 0;

  class batch {
  public:
    virtual ~batch(){};
    virtual bool set(const K& key, const V& val) = 0;
    virtual bool del(const K& key) = 0;
    virtual bool write() = 0;
    virtual bool write_sync() = 0;
    virtual bool close() = 0;
  };

  virtual std::shared_ptr<batch> new_batch() = 0;

  struct item {
    K key;
    V val;
  };

  class db_iterator_impl {
  public:
    virtual ~db_iterator_impl(){};
    virtual void increment() = 0;
    virtual const K& key() = 0;
    virtual const V& val() = 0;
  };

  class db_iterator : public std::iterator<std::forward_iterator_tag, item> {
  public:
    db_iterator(db_iterator_impl* impl) : _impl(impl){};
    db_iterator(db_iterator&& other) : _item(other._item), _impl(std::move(other._impl)) {
      other._impl = nullptr;
    }
    db_iterator(const db_iterator& other) : _item(other._item), _impl(other._impl) {
      _item.key = _impl->key();
      _item.val = _impl->val();
    }

    friend bool operator==(const db_iterator& lhs, const db_iterator& rhs) {
      return (lhs._item.key == rhs._item.key) && (lhs._item.val == rhs._item.val);
    }

    friend bool operator!=(const db_iterator& lhs, const db_iterator& rhs) {
      return !(lhs == rhs);
    }
    virtual item& operator*() {
      return _item;
    }

    virtual item* operator->() {
      return &_item;
    }

    db_iterator& operator++() {
      _impl->increment();
      _item.key = _impl->key();
      _item.val = _impl->val();
      return *this;
    };

    db_iterator operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
    };

  protected:
    item _item;

  private:
    std::shared_ptr<db_iterator_impl> _impl;
  }; // class db_iterator

  virtual db_iterator lower_bound(const K&) = 0;
  virtual db_iterator upper_bound(const K&) = 0;
}; // class db

template<typename K, typename V>
class simple_db : public virtual db<K, V> {
private:
  class simple_db_impl {
  public:
    bool get(const K& key, V& val) const {
      if (auto iter = _map.find(key); iter != _map.end()) {
        noir::p2p::bytes tmp(iter->second.begin(), iter->second.end());
        val = tmp;
        return true;
      }
      return false;
    }

    bool has(const K& key, bool& val) const {
      auto iter = _map.find(key);
      val = (iter != _map.end());
      return true;
    }

    bool set(const K& key, const V& val) {
      _map.emplace(key, val);
      return true;
    }

    bool set_sync(const K& key, const V& val) {
      set(key, val);
      return true;
    }

    bool del(const K& key) {
      _map.erase(key);
      return true;
    }

    bool del_sync(const K& key) {
      del(key);
      return true;
    }

    bool close() {
      return true;
    }

    bool print() const {
      return true;
    }

    const std::map<K, V>& stats() {
      return _map;
    }

    typename std::map<K, V>::iterator lower_bound(const K& key) {
      return _map.lower_bound(key);
    }

    typename std::map<K, V>::iterator upper_bound(const K& key) {
      return _map.upper_bound(key);
    }

  private:
    std::map<noir::p2p::bytes, noir::p2p::bytes> _map;
  }; // class simple_db_impl

  std::shared_ptr<simple_db_impl> _impl;

public:
  simple_db() : _impl(new simple_db_impl) {}

  simple_db(const simple_db& other) : _impl(std::make_shared<simple_db_impl>(*other._impl)) {}

  simple_db(simple_db&& other) : _impl(std::move(other._impl)) {
    other._impl = nullptr;
  }

  ~simple_db() override {}

  bool get(const K& key, V& val) const override {
    return _impl->get(key, val);
  }

  bool has(const K& key, bool& val) const override {
    return _impl->has(key, val);
  }

  bool set(const K& key, const V& val) override {
    return _impl->set(key, val);
  }

  bool set_sync(const K& key, const V& val) override {
    return _impl->set_sync(key, val);
  }

  bool del(const K& key) override {
    return _impl->del(key);
  }

  bool del_sync(const K& key) override {
    return _impl->del_sync(key);
  }

  bool close() override {
    return _impl->close();
  }

  bool print() const override {
    return _impl->print();
  }

  const std::map<K, V>& stats() override {
    return _impl->stats();
  }

  class simple_db_iterator_impl : public db<K, V>::db_iterator_impl {
    friend class simple_db;

  public:
    ~simple_db_iterator_impl() override{};
    void increment() override {
      _map_iter++;
    }
    const K& key() override {
      return _map_iter->first;
    }
    const V& val() override {
      return _map_iter->second;
    }

  private:
    typename std::map<K, V>::iterator _map_iter;
    simple_db_iterator_impl(typename std::map<K, V>::iterator iter) : _map_iter(iter) {}
    simple_db_iterator_impl lower_bound(const K& key) {
      return simple_db_iterator_impl{_map_iter->lower_bound(key)};
    }

    simple_db_iterator_impl upper_bound(const K& key) {
      return simple_db_iterator_impl{_map_iter->upper_bound(key)};
    }
  }; // class simple_db_iterator_impl

  typename db<K, V>::db_iterator lower_bound(const K& key) override {
    return typename db<K, V>::db_iterator(new simple_db_iterator_impl{_impl->lower_bound(key)});
  }

  typename db<K, V>::db_iterator upper_bound(const K& key) override {
    return typename db<K, V>::db_iterator(new simple_db_iterator_impl{_impl->upper_bound(key)});
  }

  class simple_db_batch : public virtual db<K, V>::batch {
  public:
    explicit simple_db_batch(std::shared_ptr<simple_db_impl> db_impl) : _db(std::move(db_impl)) {}

    explicit simple_db_batch(const simple_db_batch& other) noexcept : _db(other._db), _map(other._map) {}

    explicit simple_db_batch(simple_db_batch&& other) noexcept : _db(std::move(other._db)) {
      other._db = nullptr;
    }

    ~simple_db_batch() override{};

    bool set(const K& key, const V& val) override {
      if (_is_closed) {
        return false;
      }
      _map.emplace(key, std::make_pair(true, val));
      return true;
    }

    bool del(const K& key) override {
      if (_is_closed) {
        return false;
      }
      _map.emplace(key, std::make_pair(false, noir::p2p::bytes{}));
      return true;
    }

    bool write() override {
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

    bool write_sync() override {
      return write();
    }

    bool close() override {
      _is_closed = true;
      return true;
    }

  private:
    std::shared_ptr<simple_db_impl> _db;
    std::map<noir::p2p::bytes, std::pair<bool, noir::p2p::bytes>> _map;
    bool _is_closed = false;
  }; // class simple_db_batch

  std::shared_ptr<typename db<K, V>::batch> new_batch() override {
    return std::make_shared<simple_db_batch>(simple_db_batch(_impl));
  }
}; // class simple_db

} // namespace noir::consensus
