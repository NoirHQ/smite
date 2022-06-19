// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>

#include <noir/core/as_result.h>
#include <noir/net/detail/message_buffer.h>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/completion_condition.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>
#include <memory>
#include <span>
#include <string_view>

namespace noir::net {

template<typename Derived>
class Conn : public std::enable_shared_from_this<Derived> {
protected:
  Conn(std::string_view address, boost::asio::io_context& io_context)
    : address(address), strand(io_context.get_executor()) {}
  explicit Conn(boost::asio::io_context& io_context): strand(io_context.get_executor()) {}

public:
  template<typename Buffer>
  auto write(Buffer&& buffers) -> boost::asio::awaitable<Result<std::size_t>> {
    if (!buffers.size())
      co_return 0;

    auto bytes_transferred = co_await boost::asio::async_write(*static_cast<Derived*>(this)->socket,
      buffers,
      as_result(boost::asio::use_awaitable));
    if (!bytes_transferred)
      co_return bytes_transferred.error();
    co_return bytes_transferred.value();
  }

  auto write(std::span<const unsigned char> buffer) -> boost::asio::awaitable<Result<std::size_t>> {
    auto res = co_await write(boost::asio::const_buffer(buffer.data(), buffer.size()));
    co_return res;
  }

  auto read(std::span<unsigned char> buffer) -> boost::asio::awaitable<Result<void>> {
    auto size = buffer.size();
    if (!size)
      co_return success();

    auto bytes_in_buffer = message_buffer.bytes_to_read();

    // bytes to read already in message buffer
    if (bytes_in_buffer >= size) {
      std::memcpy(buffer.data(), message_buffer.read_ptr(), size);
      message_buffer.advance_read_ptr(size);
      co_return success();
    }

    auto bytes_to_read = size - bytes_in_buffer;
    auto completion_condition = [conn = static_cast<Derived*>(this)->shared_from_this(),
                                  bytes_to_read](const boost::system::error_code& ec,
                                  size_t bytes_transferred) -> size_t {
      if (ec || bytes_transferred >= bytes_to_read) {
        return 0;
      } else {
        return bytes_to_read - bytes_transferred;
      }
    };

    // grow buffer size if insufficient
    if (message_buffer.bytes_to_write() < bytes_to_read) {
      message_buffer.add_space(bytes_to_read - message_buffer.bytes_to_write());
    }

    auto weak_conn = static_cast<Derived*>(this)->weak_from_this();

    auto bufs = message_buffer.get_buffer_sequence_for_boost_async_read();
    if (!bufs) {
      co_return bufs.error();
    }

    auto bytes_transferred = co_await boost::asio::async_read(*static_cast<Derived*>(this)->socket,
      bufs.value(),
      completion_condition,
      as_result(boost::asio::use_awaitable));
    if (!bytes_transferred)
      co_return bytes_transferred.error();

    auto conn = weak_conn.lock();
    conn->message_buffer.advance_write_ptr(bytes_transferred.value());
    if (conn->message_buffer.bytes_to_read() < size) {
      co_return Error::format("failed to read requested bytes: {} < {}", conn->message_buffer.bytes_to_read(), size);
    }
    std::memcpy(buffer.data(), conn->message_buffer.read_ptr(), size);
    conn->message_buffer.advance_read_ptr(size);
    co_return success();
  }

  auto read(uint8_t* c, std::size_t s) -> boost::asio::awaitable<Result<void>> {
    return read(std::span{c, s});
  }

  auto close() -> Result<void> {
    return static_cast<Derived*>(this)->close();
  }

  auto connect() -> boost::asio::awaitable<Result<void>> {
    return static_cast<Derived*>(this)->connect();
  }

  boost::asio::strand<boost::asio::io_context::executor_type> strand;
  detail::message_buffer<1024 * 1024> message_buffer;
  std::string address;
};

} // namespace noir::net
