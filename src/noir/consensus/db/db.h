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

  class db_iterator_impl {
  public:
    virtual ~db_iterator_impl(){};
    virtual void increment() = 0;
    virtual void decrement() = 0;
    virtual const K& key() const = 0;
    virtual const V& val() const = 0;
    virtual db_iterator_impl* clone() = 0;
  };

  class db_iterator : public std::iterator<std::bidirectional_iterator_tag, const K> {
  public:
    db_iterator(db_iterator_impl* impl) : impl_(impl){};
    db_iterator(db_iterator&& other) : impl_(std::move(other.impl_)) {
      other.impl_ = nullptr;
    }
    db_iterator(const db_iterator& other) : impl_(other.impl_->clone()) {}

    friend bool operator==(const db_iterator& lhs, const db_iterator& rhs) {
      return (lhs.impl_->key() == rhs.impl_->key()) && (lhs.impl_->val() == rhs.impl_->val());
    }

    friend bool operator!=(const db_iterator& lhs, const db_iterator& rhs) {
      return !(lhs == rhs);
    }
    virtual const V& operator*() {
      return impl_->val();
    }

    virtual const V* operator->() {
      return &impl_->val();
    }

    db_iterator& operator++() {
      impl_->increment();
      return *this;
    }

    db_iterator operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }

    db_iterator& operator--() {
      impl_->decrement();
      return *this;
    }

    db_iterator operator--(int) {
      auto tmp = *this;
      --(*this);
      return tmp;
    }
    const K& key() const {
      return impl_->key();
    }
    const V& val() const {
      return impl_->val();
    }

  private:
    std::unique_ptr<db_iterator_impl> impl_;
  }; // class db_iterator
  using db_reverse_iterator = std::reverse_iterator<db_iterator>;

  virtual db_iterator begin_iterator(const K&) = 0;
  virtual db_iterator end_iterator(const K&) = 0;

  virtual db_reverse_iterator rbegin_iterator(const K&) = 0;
  virtual db_reverse_iterator rend_iterator(const K&) = 0;
}; // class db

template<typename K, typename V>
class simple_db : public virtual db<K, V> {
private:
  class simple_db_impl {
  public:
    bool get(const K& key, V& val) const {
      if (auto iter = map_.find(key); iter != map_.end()) {
        noir::p2p::bytes tmp(iter->second.begin(), iter->second.end());
        val = tmp;
        return true;
      }
      return false;
    }

    bool has(const K& key, bool& val) const {
      auto iter = map_.find(key);
      val = (iter != map_.end());
      return true;
    }

    bool set(const K& key, const V& val) {
      map_.emplace(key, val);
      return true;
    }

    bool set_sync(const K& key, const V& val) {
      set(key, val);
      return true;
    }

    bool del(const K& key) {
      map_.erase(key);
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
      return map_;
    }

    typename std::map<K, V>::iterator lower_bound(const K& key) {
      return map_.lower_bound(key);
    }

    typename std::map<K, V>::iterator upper_bound(const K& key) {
      return map_.upper_bound(key);
    }

  private:
    std::map<noir::p2p::bytes, noir::p2p::bytes> map_;
  }; // class simple_db_impl

  std::shared_ptr<simple_db_impl> impl_;

public:
  simple_db() : impl_(new simple_db_impl) {}

  simple_db(const simple_db& other) : impl_(std::make_shared<simple_db_impl>(*other.impl_)) {}

  simple_db(simple_db&& other) : impl_(std::move(other.impl_)) {
    other.impl_ = nullptr;
  }

  ~simple_db() override {}

  bool get(const K& key, V& val) const override {
    return impl_->get(key, val);
  }

  bool has(const K& key, bool& val) const override {
    return impl_->has(key, val);
  }

  bool set(const K& key, const V& val) override {
    return impl_->set(key, val);
  }

  bool set_sync(const K& key, const V& val) override {
    return impl_->set_sync(key, val);
  }

  bool del(const K& key) override {
    return impl_->del(key);
  }

  bool del_sync(const K& key) override {
    return impl_->del_sync(key);
  }

  bool close() override {
    return impl_->close();
  }

  bool print() const override {
    return impl_->print();
  }

  const std::map<K, V>& stats() override {
    return impl_->stats();
  }

  class simple_db_iterator_impl : public db<K, V>::db_iterator_impl {
    friend class simple_db;

  public:
    ~simple_db_iterator_impl() override{};
    void increment() override {
      map_iter_++;
    }
    void decrement() override {
      map_iter_--;
    }
    const K& key() const override {
      return map_iter_->first;
    }
    const V& val() const override {
      return map_iter_->second;
    }
    typename db<K, V>::db_iterator_impl* clone() override {
      return new simple_db_iterator_impl(map_iter_);
    }

  private:
    typename std::map<K, V>::iterator map_iter_;
    simple_db_iterator_impl(typename std::map<K, V>::iterator iter) : map_iter_(iter) {}
    simple_db_iterator_impl lower_bound(const K& key) {
      return simple_db_iterator_impl{map_iter_->lower_bound(key)};
    }

    simple_db_iterator_impl upper_bound(const K& key) {
      return simple_db_iterator_impl{map_iter_->upper_bound(key)};
    }
  }; // class simple_db_iterator_impl

  typename db<K, V>::db_iterator begin_iterator(const K& key) override {
    return typename db<K, V>::db_iterator(new simple_db_iterator_impl{impl_->lower_bound(key)});
  }

  typename db<K, V>::db_iterator end_iterator(const K& key) override {
    return typename db<K, V>::db_iterator(new simple_db_iterator_impl{impl_->upper_bound(key)});
  }

  typename db<K, V>::db_reverse_iterator rbegin_iterator(const K& key) override {
    return std::make_reverse_iterator(end_iterator(key));
  }

  typename db<K, V>::db_reverse_iterator rend_iterator(const K& key) override {
    return std::make_reverse_iterator(begin_iterator(key));
  }

  class simple_db_batch : public virtual db<K, V>::batch {
  public:
    explicit simple_db_batch(std::shared_ptr<simple_db_impl> db_impl) : db_(std::move(db_impl)) {}

    explicit simple_db_batch(const simple_db_batch& other) noexcept : db_(other.db_), map_(other.map_) {}

    explicit simple_db_batch(simple_db_batch&& other) noexcept : db_(std::move(other.db_)) {
      other.db_ = nullptr;
    }

    ~simple_db_batch() override{};

    bool set(const K& key, const V& val) override {
      if (is_closed_) {
        return false;
      }
      map_.emplace(key, std::make_pair(true, val));
      return true;
    }

    bool del(const K& key) override {
      if (is_closed_) {
        return false;
      }
      map_.emplace(key, std::make_pair(false, noir::p2p::bytes{}));
      return true;
    }

    bool write() override {
      if (is_closed_) {
        return false;
      }
      std::for_each(map_.begin(), map_.end(), [&](const auto& t) {
        if (t.second.first == true) {
          db_->set(t.first, t.second.second);
        } else {
          db_->del(t.first);
        }
      });
      return true;
    }

    bool write_sync() override {
      return write();
    }

    bool close() override {
      is_closed_ = true;
      return true;
    }

  private:
    std::shared_ptr<simple_db_impl> db_;
    std::map<noir::p2p::bytes, std::pair<bool, noir::p2p::bytes>> map_;
    bool is_closed_ = false;
  }; // class simple_db_batch

  std::shared_ptr<typename db<K, V>::batch> new_batch() override {
    return std::make_shared<simple_db_batch>(simple_db_batch(impl_));
  }
}; // class simple_db

} // namespace noir::consensus
