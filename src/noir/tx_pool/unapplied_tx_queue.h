// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types.h>
#include <noir/consensus/tx.h>
#include <noir/p2p/types.h>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>

namespace noir::tx_pool {

struct unapplied_tx {
  const consensus::wrapped_tx_ptr tx_ptr;

  const uint64_t gas() const {
    return tx_ptr->gas;
  }
  const uint64_t nonce() const {
    return tx_ptr->nonce;
  }
  const consensus::address_type sender() const {
    return tx_ptr->sender;
  }
  const std::string hash() const {
    return tx_ptr->hash().to_string();
  }
  const uint64_t height() const {
    return tx_ptr->height;
  }
  const p2p::tstamp time_stamp() const {
    return tx_ptr->time_stamp;
  }

  unapplied_tx(const unapplied_tx&) = delete;
  unapplied_tx() = delete;
  unapplied_tx& operator=(const unapplied_tx&) = delete;
  unapplied_tx(unapplied_tx&&) = default;
  explicit unapplied_tx(const consensus::wrapped_tx_ptr& wtx): tx_ptr(wtx) {}
};

class unapplied_tx_queue {
public:
  struct by_hash;
  struct by_gas;
  struct by_sender;
  struct by_nonce;
  struct by_height;
  struct by_time;

private:
  // clang-format off
  typedef boost::multi_index::multi_index_container<
    unapplied_tx,
    boost::multi_index::indexed_by<
      boost::multi_index::hashed_unique<
        boost::multi_index::tag<by_hash>,
        boost::multi_index::const_mem_fun<unapplied_tx, const std::string, &unapplied_tx::hash>
      >,
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<by_gas>,
        boost::multi_index::const_mem_fun<unapplied_tx, const uint64_t, &unapplied_tx::gas>
      >,
      boost::multi_index::hashed_non_unique<
        boost::multi_index::tag<by_sender>,
        boost::multi_index::const_mem_fun<unapplied_tx, const consensus::address_type, &unapplied_tx::sender>
      >,
      boost::multi_index::ordered_unique<
        boost::multi_index::tag<by_nonce>,
        boost::multi_index::composite_key<unapplied_tx,
          boost::multi_index::const_mem_fun<unapplied_tx, const consensus::address_type, &unapplied_tx::sender>,
          boost::multi_index::const_mem_fun<unapplied_tx, const uint64_t, &unapplied_tx::nonce>
        >
      >,
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<by_height>,
        boost::multi_index::const_mem_fun<unapplied_tx, const uint64_t, &unapplied_tx::height>
      >,
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<by_time>,
        boost::multi_index::const_mem_fun<unapplied_tx, const p2p::tstamp, &unapplied_tx::time_stamp>
      >
    >
  > unapplied_tx_queue_type;
  // clang-format on

  unapplied_tx_queue_type queue_;
  uint64_t max_tx_queue_bytes_size_ = 1024 * 1024 * 1024;
  uint64_t size_in_bytes_ = 0;
  size_t incoming_count_ = 0;

public:
  unapplied_tx_queue() = default;

  unapplied_tx_queue(uint64_t size) {
    max_tx_queue_bytes_size_ = size;
  }

  bool empty() const {
    return queue_.empty();
  }

  size_t size() const {
    return queue_.size();
  }

  uint64_t bytes_size() const {
    return size_in_bytes_;
  }

  void clear() {
    queue_.clear();
    size_in_bytes_ = 0;
    incoming_count_ = 0;
  }

  size_t incoming_size() const {
    return incoming_count_;
  }

  bool has(const consensus::tx_hash& tx_hash) const {
    return queue_.get<by_hash>().find(tx_hash.to_string()) != queue_.get<by_hash>().end();
  }

  consensus::wrapped_tx_ptr get_tx(const consensus::tx_hash& tx_hash) const {
    auto itr = queue_.get<by_hash>().find(tx_hash.to_string());
    if (itr == queue_.get<by_hash>().end()) {
      return {};
    }
    return itr->tx_ptr;
  }

  consensus::wrapped_tx_ptr get_tx(const consensus::address_type& sender, uint64_t nonce) const {
    auto itr = queue_.get<by_nonce>().find(std::make_tuple(sender, nonce));
    if (itr == queue_.get<by_nonce>().end()) {
      return {};
    }
    return itr->tx_ptr;
  }

  bool add_tx(const consensus::wrapped_tx_ptr& tx_ptr) {
    auto size = bytes_size(tx_ptr);
    if (size_in_bytes_ + size > max_tx_queue_bytes_size_) {
      return false;
    }

    auto res = queue_.insert(unapplied_tx{tx_ptr});
    if (res.second) {
      size_in_bytes_ += bytes_size(tx_ptr);
      incoming_count_++;
    }

    return true;
  }

  template<typename Tag>
  using iterator = typename unapplied_tx_queue_type::index<Tag>::type::iterator;

  template<typename Tag>
  using reverse_iterator = typename unapplied_tx_queue_type::index<Tag>::type::reverse_iterator;

  iterator<by_time> begin() {
    return begin<by_time>();
  }

  iterator<by_time> end() {
    return end<by_time>();
  }

  template<typename Tag>
  iterator<Tag> begin() {
    return queue_.get<Tag>().begin();
  }

  template<typename Tag>
  iterator<Tag> end() {
    return queue_.get<Tag>().end();
  }

  iterator<by_nonce> begin(const consensus::address_type& sender, const uint64_t begin = 0) {
    return queue_.get<by_nonce>().lower_bound(std::make_tuple(sender, begin));
  }

  iterator<by_nonce> end(
    const consensus::address_type& sender, const uint64_t end = std::numeric_limits<uint64_t>::max()) {
    return queue_.get<by_nonce>().upper_bound(std::make_tuple(sender, end));
  }

  template<typename Tag>
  reverse_iterator<Tag> rbegin() {
    return queue_.get<Tag>().rbegin();
  }

  template<typename Tag>
  reverse_iterator<Tag> rend() {
    return queue_.get<Tag>().rend();
  }

  template<typename Tag, typename ValType>
  iterator<Tag> begin(const ValType& val) {
    return queue_.get<Tag>().lower_bound(val);
  }

  template<typename Tag, typename ValType>
  iterator<Tag> end(const ValType& val) {
    return queue_.get<Tag>().upper_bound(val);
  }

  template<typename Tag, typename ValType>
  reverse_iterator<Tag> rbegin(const ValType& val) {
    return reverse_iterator<Tag>(queue_.get<Tag>().upper_bound(val));
  }

  template<typename Tag, typename ValType>
  reverse_iterator<Tag> rend(const ValType& val) {
    return reverse_iterator<Tag>(queue_.get<Tag>().lower_bound(val));
  }

  bool erase(const consensus::wrapped_tx_ptr& tx_ptr) {
    return erase(tx_ptr->hash().to_string());
  }

  bool erase(const consensus::tx_hash& tx_hash) {
    return erase(tx_hash.to_string());
  }

  bool erase(const std::string& tx_hash) {
    auto itr = queue_.get<by_hash>().find(tx_hash);
    if (itr == queue_.get<by_hash>().end()) {
      return false;
    }

    incoming_count_--;
    size_in_bytes_ -= bytes_size(itr->tx_ptr);
    queue_.get<by_hash>().erase(itr);
    return true;
  }

  static uint64_t bytes_size(const consensus::wrapped_tx_ptr& tx_ptr) {
    return sizeof(unapplied_tx) + tx_ptr->size();
  }
};

} // namespace noir::tx_pool
