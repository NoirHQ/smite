// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types.h>
#include <noir/consensus/tx.h>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index_container.hpp>

namespace noir::tx_pool {

struct unapplied_tx {
  const consensus::tx_ptr tx_ptr;

  const uint64_t gas() const { return tx_ptr->gas; }
  const uint64_t nonce() const { return tx_ptr->nonce; }
  const consensus::sender_type sender() const { return tx_ptr->sender; }
  const consensus::tx_id_type id() const { return tx_ptr->id(); }

//  unapplied_tx(const unapplied_tx&) = delete;
//  unapplied_tx() = delete;
//  unapplied_tx& operator=(const unapplied_tx&) = delete;
//  unapplied_tx(unapplied_tx&&) = default;
//  unapplied_tx(const consensus::tx_ptr, uint64_t, uint64_t);
};


class unapplied_tx_queue {
public:
  struct by_tx_id;
  struct by_gas;
  struct by_nonce;

private:
  typedef boost::multi_index::multi_index_container<
    unapplied_tx,
    boost::multi_index::indexed_by<
      boost::multi_index::ordered_unique<
        boost::multi_index::tag<by_tx_id>,
        boost::multi_index::const_mem_fun<unapplied_tx, const consensus::tx_id_type, &unapplied_tx::id>
      >,
      boost::multi_index::ordered_non_unique<
        boost::multi_index::tag<by_gas>,
        boost::multi_index::const_mem_fun<unapplied_tx, const uint64_t, &unapplied_tx::gas>
      >,
      boost::multi_index::ordered_unique<
        boost::multi_index::tag<by_nonce>,
        boost::multi_index::composite_key<
          unapplied_tx,
          boost::multi_index::const_mem_fun<unapplied_tx, const consensus::sender_type, &unapplied_tx::sender>,
          boost::multi_index::const_mem_fun<unapplied_tx, const uint64_t, &unapplied_tx::nonce>
        >
      >
    >
  > unapplied_tx_queue_type;

  unapplied_tx_queue_type queue_;
  uint64_t max_tx_queue_size_ = 1024 * 1024;
  uint64_t size_in_bytes_ = 0;
  size_t incoming_count_ = 0;

public:
  unapplied_tx_queue() = default;

  explicit unapplied_tx_queue(uint64_t size) { max_tx_queue_size_ = size; }

  bool empty() const {
    return queue_.empty();
  }

  size_t size() const {
    return queue_.size();
  }

  void clear() {
    queue_.clear();
  }

  size_t incoming_size() const {
    return incoming_count_;
  }

  consensus::tx_ptr get_tx(const consensus::tx_id_type& id) const {
    auto itr = queue_.get<by_tx_id>().find(id);
    if (itr == queue_.get<by_tx_id>().end()) {
      return {};
    }
    return itr->tx_ptr;
  }

  bool add_tx(const consensus::tx_ptr& tx_ptr) {
    auto itr = queue_.get<by_tx_id>().find(tx_ptr->id());
    if (itr != queue_.get<by_tx_id>().end()) {
      return false;
    }
    auto insert_itr = queue_.insert({tx_ptr});
    if (insert_itr.second) {
      size_in_bytes_ += bytes_size(tx_ptr);
    }

    return insert_itr.second;
  }

  template<typename Tag>
  using iterator = typename unapplied_tx_queue_type::index<Tag>::type::iterator;

  template<typename Tag>
  using reverse_iterator = typename unapplied_tx_queue_type::index<Tag>::type::reverse_iterator;

  template<typename Tag>
  iterator<Tag> begin() { return queue_.get<Tag>().begin(); }

  template<typename Tag>
  iterator<Tag> end() { return queue_.get<Tag>().end(); }

  template<typename Tag>
  reverse_iterator<Tag> rbegin() { return queue_.get<Tag>().rbegin(); }

  template<typename Tag>
  reverse_iterator<Tag> rend() { return queue_.get<Tag>().rend(); }

  template<typename Tag, typename ValType>
  iterator<Tag> lower_bound(const ValType& val) {
    return queue_.get<Tag>().lower_bound(val);
  }

  template<typename Tag, typename ValType>
  iterator<Tag> upper_bound(const ValType& val) {
    return queue_.get<Tag>().upper_bound(val);
  }

  static uint64_t bytes_size(const consensus::tx_ptr& tx_ptr) {
    return sizeof(unapplied_tx) + tx_ptr->size();
  }

};

} // namespace noir::mempool
