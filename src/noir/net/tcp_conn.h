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
  using super = Conn<TcpConn>;

  template<typename Executor>
  TcpConn(Executor&& ex, std::string_view address): super(ex, address), socket(new boost::asio::ip::tcp::socket(ex)) {}

  TcpConn(boost::asio::ip::tcp::socket&& socket)
    : super(socket.get_executor()), socket(std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket))) {
    boost::system::error_code ec;

    auto remote = this->socket->remote_endpoint(ec);
    auto host = ec ? "<unknown>" : remote.address().to_string();
    auto port = ec ? "<unknown>" : std::to_string(remote.port());
    super::address = host + ":" + port;
  }

public:
  template<typename Executor>
  [[nodiscard]] static auto create(Executor&& executor, std::string_view address) {
    return std::shared_ptr<TcpConn>(new TcpConn(executor, address));
  }

  [[nodiscard]] static auto create(std::string_view address) {
    return std::shared_ptr<TcpConn>(new TcpConn(eo::runtime::execution_context, address));
  }

  [[nodiscard]] static auto create(boost::asio::ip::tcp::socket&& socket) {
    return std::shared_ptr<TcpConn>(new TcpConn(std::move(socket)));
  }

  auto connect() -> boost::asio::awaitable<Result<void>> {
    auto weak_conn = super::weak_from_this();

    auto colon = super::address.find(':');
    if (colon == std::string::npos) {
      co_return Error::format("failed to parse address: {}", super::address);
    }
    auto host = super::address.substr(0, colon);
    auto port = super::address.substr(colon + 1);

    auto resolver = std::make_shared<boost::asio::ip::tcp::resolver>(super::strand);
    auto endpoints =
      co_await resolver->async_resolve(boost::asio::ip::tcp::v4(), host, port, as_result(boost::asio::use_awaitable));
    if (!endpoints)
      co_return endpoints.error();

    auto conn = weak_conn.lock();
    auto endpoint =
      co_await boost::asio::async_connect(*conn->socket, endpoints.value(), as_result(boost::asio::use_awaitable));
    if (!endpoint)
      co_return endpoint.error();
    co_return success();
  }

public:
  std::shared_ptr<boost::asio::ip::tcp::socket> socket;
};

template<typename T>
auto new_tcp_conn(T& ex, std::string_view address) {
  if constexpr (ExecutionContext<T>) {
    auto executor = ex.get_executor();
    return TcpConn::create(executor, address);
  } else {
    return TcpConn::create(ex, address);
  }
}

auto new_tcp_conn(std::string_view address) {
  return new_tcp_conn(eo::runtime::execution_context, address);
}

} // namespace noir::net
