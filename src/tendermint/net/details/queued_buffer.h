// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once
#include <boost/asio/buffer.hpp>
#include <boost/noncopyable.hpp>
#include <boost/system/error_code.hpp>
#include <deque>
#include <mutex>

namespace tendermint::net::details {

// thread safe
class queued_buffer : boost::noncopyable {
public:
  void clear_write_queue();
  void clear_out_queue();
  uint32_t write_queue_size() const;
  bool is_out_queue_empty() const;
  bool ready_to_send() const;
  // @param callback must not callback into queued_buffer
  bool add_write_queue(const std::shared_ptr<std::vector<char>>& buff,
    std::function<void(boost::system::error_code, std::size_t)> callback, bool to_sync_queue);
  void fill_out_buffer(std::vector<boost::asio::const_buffer>& bufs);
  void out_callback(boost::system::error_code ec, std::size_t w);

private:
  struct queued_write;
  void fill_out_buffer(std::vector<boost::asio::const_buffer>& bufs, std::deque<queued_write>& w_queue);

private:
  struct queued_write {
    std::shared_ptr<std::vector<char>> buff;
    std::function<void(boost::system::error_code, std::size_t)> callback;
  };

  mutable std::mutex _mtx;
  uint32_t _write_queue_size{0};
  std::deque<queued_write> _write_queue;
  std::deque<queued_write> _sync_write_queue; // sync_write_queue will be sent first
  std::deque<queued_write> _out_queue;
};

} // namespace tendermint::net::details
