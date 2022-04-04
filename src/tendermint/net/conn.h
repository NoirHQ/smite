// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <tendermint/common/common.h>
#include <tendermint/log/log.h>
#include <tendermint/net/details/message_buffer.hpp>
#include <tendermint/net/details/queued_buffer.h>
#include <boost/asio/io_context_strand.hpp>
#include <future>
#include <memory>
#include <string>

namespace tendermint::net {

using ConnectHandler = std::function<void(const boost::system::error_code&)>;

template<typename Derived>
class Conn : public std::enable_shared_from_this<Derived> {
private:
  Conn(const std::string& address, boost::asio::io_context& io_context): address(address), strand(io_context) {}

  friend Derived;

public:
  result<void> connect() {
    return static_cast<Derived*>(this)->on_connect();
  }

  std::future<result<void>> async_connect(ConnectHandler&& ch = {}) {
    return static_cast<Derived*>(this)->on_async_connect(std::move(ch));
  }

  boost::asio::io_context::strand strand;

  details::message_buffer<1024 * 1024> pending_message_buffer;
  std::atomic<std::size_t> outstanding_read_bytes{0};

private:
  const std::string address;
};

} // namespace tendermint::net
