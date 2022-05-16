// This file is part of NOIR.
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/net/conn.h>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace noir::net {

class TcpConn : public Conn<TcpConn> {
private:
  TcpConn(std::string_view address, boost::asio::io_context& io_context)
    : Conn(address, io_context), socket(new boost::asio::ip::tcp::socket(io_context.get_executor())) {}

  explicit TcpConn(boost::asio::ip::tcp::socket&& socket)
    : Conn(static_cast<boost::asio::io_context&>(socket.get_executor().context())),
      socket(std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket))) {
    boost::system::error_code ec;

    auto remote = this->socket->remote_endpoint(ec);
    auto host = ec ? "<unknown>" : remote.address().to_string();
    auto port = ec ? "<unknown>" : std::to_string(remote.port());
    address = host + ":" + port;
  }

public:
  [[nodiscard]] static auto create(std::string_view address, boost::asio::io_context& io_context)
    -> std::shared_ptr<TcpConn> {
    return std::shared_ptr<TcpConn>(new TcpConn(address, io_context));
  }

  [[nodiscard]] static auto create(boost::asio::ip::tcp::socket&& socket) -> std::shared_ptr<TcpConn> {
    return std::shared_ptr<TcpConn>(new TcpConn(std::move(socket)));
  }

  auto connect() -> boost::asio::awaitable<Result<void>> {
    auto weak_conn = weak_from_this();

    auto colon = address.find(':');
    if (colon == std::string::npos) {
      co_return Error::format("failed to parse address: {}", address);
    }
    auto host = address.substr(0, colon);
    auto port = address.substr(colon + 1);

    auto resolver = std::make_shared<boost::asio::ip::tcp::resolver>(strand);
    auto endpoints = co_await resolver->async_resolve(boost::asio::ip::tcp::v4(),
      host,
      port,
      as_result(boost::asio::use_awaitable));
    if (!endpoints)
      co_return endpoints.error();

    auto conn = weak_conn.lock();
    auto endpoint = co_await boost::asio::async_connect(*conn->socket,
      endpoints.value(),
      as_result(boost::asio::use_awaitable));
    if (!endpoint)
      co_return endpoint.error();
    co_return success();
  }

  auto close() -> Result<void> {
    boost::system::error_code ec{};
    socket->close(ec);
    if (ec) {
      return ec;
    }
    return success();
  }

public:
  std::shared_ptr<boost::asio::ip::tcp::socket> socket;
};

} // namespace noir::net
