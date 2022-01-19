// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <noir/p2p/types.h>

namespace noir::consensus {

/// \addtogroup consensus
/// \{

/// \brief wrapper of database
template<typename K, typename V>
class db {
public:
  virtual ~db(){};
  /// \brief gets stored value with key
  /// \param[in] key key to get stored value
  /// \param[out] val stored value to get
  /// \return true on success, false otherwise
  virtual bool get(const K& key, V& val) const = 0;
  /// \brief checks whether key exists
  /// \param[in] key key to check
  /// \param[out] val true if exists, false otherwise
  /// \return true on success, false otherwise
  virtual bool has(const K& key, bool& val) const = 0;
  /// \brief sets value with key
  /// \param[in] key key to store value
  /// \param[in] val value to store
  /// \return true on success, false otherwise
  virtual bool set(const K& key, const V& val) = 0;
  /// \brief sets given value with key, and flushes it to storage
  /// \param[in] key key to store value
  /// \param[in] val value to store
  /// \return true on success, false otherwise
  virtual bool set_sync(const K& key, const V& val) = 0;
  /// \brief deletes stored value with key
  /// \param[in] key key to delete
  /// \return true on success, false otherwise
  virtual bool del(const K& key) = 0;
  /// \brief deletes stored value with given key, and flushes it to storage
  /// \param[in] key key to delete
  /// \return true on success, false otherwise
  virtual bool del_sync(const K& key) = 0;
  /// \brief closes database connection
  /// \return true on success, false otherwise
  virtual bool close() = 0;
  /// prints info for debug
  /// \return true on success, false otherwise
  virtual bool print() const = 0;
  /// provides a map of property values for keys
  /// \return map object
  virtual const std::map<K, V>& stats() = 0;

  /// \brief group of write for db
  class batch {
  public:
    virtual ~batch(){};
    /// \brief gets stored value with key
    /// \param[in] key key to get stored value
    /// \param[out] val stored value to get
    /// \return true on success, false otherwise
    virtual bool set(const K& key, const V& val) = 0;
    /// \brief deletes stored value with given key, and flushes it to storage
    /// \param[in] key key to delete
    /// \return true on success, false otherwise
    virtual bool del(const K& key) = 0;
    /// \brief writes the batch
    /// \note Only close() can be called after calling this method
    /// \return true on success, false otherwise
    virtual bool write() = 0;
    /// \brief writes the batch and flushes it to storage
    /// \note Only close() can be called after calling this method
    /// \return true on success, false otherwise
    virtual bool write_sync() = 0;
    /// \brief closes the batch
    /// \return true on success, false otherwise
    virtual bool close() = 0;
  };

  /// \brief creates a batch
  /// \return shared_ptr of batch
  virtual std::shared_ptr<batch> new_batch() = 0;

  /// \brief internal implementation of db_iterator and wrapper of iterator of internal container
  class db_iterator_impl {
  public:
    virtual ~db_iterator_impl(){};
    /// \brief increase internal iterator
    virtual void increment() = 0;
    /// \brief decrease internal iterator
    virtual void decrement() = 0;
    /// \brief gets key of internal iterator is pointing
    /// \return key
    virtual const K& key() const = 0;
    /// \brief gets value of internal iterator is pointing
    /// \return value
    virtual const V& val() const = 0;
    /// \brief clones self
    /// \return  pointer of new db_iterator_impl object
    virtual db_iterator_impl* clone() = 0;

    virtual bool is_equal(const db_iterator_impl&) const = 0;
  };

  /// \brief wrapper of iterator
  template<bool reverse = false>
  class db_iterator : public std::iterator<std::bidirectional_iterator_tag, const K> {
  public:
    db_iterator(db_iterator_impl* begin, db_iterator_impl* end) : impl_(begin), begin_(begin->clone()), end_(end) {}
    db_iterator(db_iterator_impl* impl, db_iterator_impl* begin, db_iterator_impl* end)
      : impl_(impl), begin_(begin), end_(end) {}
    db_iterator(db_iterator&& other) : impl_(std::move(other.impl_)) {
      other.impl_ = nullptr;
    }
    db_iterator(const db_iterator& other) : impl_(other.impl_->clone()) {}

    friend bool operator==(const db_iterator& lhs, const db_iterator& rhs) {
      return lhs.impl_->is_equal(*rhs.impl_.get());
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
      if constexpr (reverse == false) {
        impl_->increment();
      } else {
        impl_->decrement();
      }
      return *this;
    }

    db_iterator operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }

    db_iterator& operator--() {
      if constexpr (reverse == false) {
        impl_->decrement();
      } else {
        impl_->increment();
      }
      return *this;
    }

    db_iterator operator--(int) {
      auto tmp = *this;
      --(*this);
      return tmp;
    }
    /// \brief get key of iterator
    /// \return key
    const K& key() const {
      return impl_->key();
    }
    /// \brief get value of iterator
    /// \return value
    const V& val() const {
      return impl_->val();
    }

    const db_iterator begin() {
      return db_iterator(begin_->clone(), begin_->clone(), end_->clone());
    }
    const db_iterator end() {
      return db_iterator(end_->clone(), begin_->clone(), end_->clone());
    }

  private:
    std::unique_ptr<db_iterator_impl> impl_;
    std::unique_ptr<db_iterator_impl> begin_;
    std::unique_ptr<db_iterator_impl> end_;
  }; // class db_iterator

}; // class db

/// \brief wrapper of map<K,V>
template<typename K, typename V>
class simple_db : public virtual db<K, V> {
private:
  class simple_db_impl {
    friend class simple_db;

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
      ++map_iter_;
    }
    void decrement() override {
      if (map_iter_ != map_begin_) {
        --map_iter_;
      } else {
        map_iter_ = map_end_;
      }
    }
    const K& key() const override {
      return map_iter_->first;
    }
    const V& val() const override {
      return map_iter_->second;
    }
    typename db<K, V>::db_iterator_impl* clone() override {
      return new simple_db_iterator_impl(map_iter_, map_begin_, map_end_);
    }

    bool is_equal(const typename db<K, V>::db_iterator_impl& other) const override {
      return (typeid(*this) == typeid(other)) &&
        (map_iter_ == dynamic_cast<const simple_db_iterator_impl&>(other).map_iter_);
    }

  private:
    typename std::map<K, V>::iterator map_begin_;
    typename std::map<K, V>::iterator map_end_;
    typename std::map<K, V>::iterator map_iter_;
    simple_db_iterator_impl(typename std::map<K, V>::iterator iter, typename std::map<K, V>::iterator begin,
      typename std::map<K, V>::iterator end)
      : map_iter_(iter), map_begin_(begin), map_end_(end) {}
    simple_db_iterator_impl lower_bound(const K& key) {
      return simple_db_iterator_impl{map_iter_->lower_bound(key), map_begin_, map_end_};
    }

    simple_db_iterator_impl upper_bound(const K& key) {
      return simple_db_iterator_impl{map_iter_->upper_bound(key), map_begin_, map_end_};
    }
  }; // class simple_db_iterator_impl

  template<bool reverse = false>
  typename db<K, V>::template db_iterator<reverse> get_iterator(const K& start, const K& end) {
    // begin: inclusive / end: exclusive
    auto begin_ = impl_->map_.begin();
    auto end_ = impl_->map_.end();
    if constexpr (reverse == false) {
      return typename db<K, V>::template db_iterator<false>(
        new simple_db_iterator_impl{impl_->lower_bound(start), begin_, end_},
        new simple_db_iterator_impl{impl_->upper_bound(end), begin_, end_});
    } else {
      auto start_it = impl_->upper_bound(end);
      if (start_it != begin_) {
        --start_it;
      }
      auto end_it = impl_->lower_bound(start);
      if (end_it != begin_) {
        --end_it;
      } else {
        end_it = end_;
      }
      return typename db<K, V>::template db_iterator<true>(
        new simple_db_iterator_impl{start_it, begin_, end_}, new simple_db_iterator_impl{end_it, begin_, end_});
    }
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
/// \}

} // namespace noir::consensus
