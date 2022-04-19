// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#include <noir/common/check.h>
#include <tendermint/net/config.h>
#include <tendermint/net/details/queued_buffer.h>

namespace tendermint::net::details {

void queued_buffer::clear_write_queue() {
  std::scoped_lock<std::mutex> g(_mtx);
  _write_queue.clear();
  _sync_write_queue.clear();
  _write_queue_size = 0;
}

void queued_buffer::clear_out_queue() {
  std::scoped_lock<std::mutex> g(_mtx);
  while (_out_queue.size() > 0) {
    _out_queue.pop_front();
  }
}

uint32_t queued_buffer::write_queue_size() const {
  std::scoped_lock<std::mutex> g(_mtx);
  return _write_queue_size;
}

bool queued_buffer::is_out_queue_empty() const {
  std::scoped_lock<std::mutex> g(_mtx);
  return _out_queue.empty();
}

bool queued_buffer::ready_to_send() const {
  std::scoped_lock<std::mutex> g(_mtx);
  // if out_queue is not empty then async_write is in progress
  return ((!_sync_write_queue.empty() || !_write_queue.empty()) && _out_queue.empty());
}

// @param callback must not callback into queued_buffer
bool queued_buffer::add_write_queue(const std::shared_ptr<std::vector<char>>& buff,
  std::function<void(boost::system::error_code, std::size_t)> callback,
  bool to_sync_queue) {
  std::scoped_lock<std::mutex> g(_mtx);
  if (to_sync_queue) {
    _sync_write_queue.push_back({buff, callback});
  } else {
    _write_queue.push_back({buff, callback});
  }
  _write_queue_size += buff->size();
  if (_write_queue_size > 2 * def_max_write_queue_size) {
    return false;
  }
  return true;
}

void queued_buffer::fill_out_buffer(std::vector<boost::asio::const_buffer>& bufs) {
  std::scoped_lock<std::mutex> g(_mtx);
  if (_sync_write_queue.size() > 0) { // always send msgs from sync_write_queue first
    fill_out_buffer(bufs, _sync_write_queue);
  } else { // postpone real_time write_queue if sync queue is not empty
    fill_out_buffer(bufs, _write_queue);
    noir::check(_write_queue_size == 0, "write queue size expected to be zero");
  }
}

void queued_buffer::out_callback(boost::system::error_code ec, std::size_t w) {
  std::scoped_lock<std::mutex> g(_mtx);
  for (auto& m : _out_queue) {
    m.callback(ec, w);
  }
}

void queued_buffer::fill_out_buffer(std::vector<boost::asio::const_buffer>& bufs, std::deque<queued_write>& w_queue) {
  while (w_queue.size() > 0) {
    auto& m = w_queue.front();
    bufs.push_back(boost::asio::buffer(*m.buff));
    _write_queue_size -= m.buff->size();
    _out_queue.emplace_back(m);
    w_queue.pop_front();
  }
}

} // namespace tendermint::net::details
