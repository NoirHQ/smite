// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <eo/core.h>

#include <noir/common/bytes_view.h>
#include <noir/net/detail/execution_context.h>
#include <noir/net/detail/message_buffer.h>
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
  template<typename Executor>
  Conn(Executor&& ex, std::string_view address = {}): address(address), strand(ex) {}

  template<typename Executor>
  requires(ExecutionContext<std::decay_t<Executor>>) Conn(Executor&& ex, std::string_view address = {})
    : address(address), strand(ex.get_executor()) {}

  std::string address;

public:
  auto write(boost::asio::const_buffer& buffers) -> boost::asio::awaitable<Result<void>> {
    if (!buffers.size())
      co_return success();

    auto bytes_transferred = co_await boost::asio::async_write(
      *static_cast<Derived*>(this)->socket, buffers, as_result(boost::asio::use_awaitable));
    if (!bytes_transferred)
      co_return bytes_transferred.error();
    co_return success();
  }

  template<ByteSequence T>
  auto write(T&& buffer) -> boost::asio::awaitable<Result<void>>
  requires(!std::is_same_v<T, boost::asio::const_buffer>) {
    auto buf = boost::asio::const_buffer(buffer.data(), buffer.size());
    auto res = co_await write(buf);
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
    auto completion_condition = [conn = static_cast<Derived*>(this)->shared_from_this(), bytes_to_read](
                                  const boost::system::error_code& ec, size_t bytes_transferred) -> size_t {
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

    auto bytes_transferred = co_await boost::asio::async_read(
      *static_cast<Derived*>(this)->socket, bufs.value(), completion_condition, as_result(boost::asio::use_awaitable));
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

  auto read(unsigned char* data, size_t size) -> boost::asio::awaitable<Result<void>> {
    return read(std::span{data, size});
  }

  auto is_open() {
    return static_cast<Derived*>(this)->socket->is_open();
  }

  auto close() {
    return static_cast<Derived*>(this)->socket->close();
  }

  boost::asio::strand<boost::asio::any_io_executor> strand;
  detail::message_buffer<1024 * 1024> message_buffer;
};

} // namespace noir::net
