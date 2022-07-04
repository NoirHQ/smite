// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <noir/net/tcp_conn.h>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/as_tuple.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <string_view>

namespace noir::net {

class TcpListener : public std::enable_shared_from_this<TcpListener> {
private:
  template<typename Executor>
  TcpListener(const Executor& ex): strand(ex), acceptor(ex) {}

public:
  template<typename Executor>
  [[nodiscard]] static auto create(const Executor& ex) {
    return std::shared_ptr<TcpListener>(new TcpListener(ex));
  }

  auto listen(std::string_view address) -> boost::asio::awaitable<Result<void>> {
    auto colon = address.find(':');
    if (colon == std::string::npos) {
      co_return Error::format("failed to parse address: {}", address);
    }
    auto host = address.substr(0, colon);
    auto port = address.substr(colon + 1);

    std::shared_ptr<boost::asio::ip::tcp::resolver> resolver = std::make_shared<boost::asio::ip::tcp::resolver>(strand);
    auto [ec, endpoints] = co_await resolver->async_resolve(
      boost::asio::ip::tcp::v4(), host, port, boost::asio::experimental::as_tuple(boost::asio::use_awaitable));
    if (ec)
      co_return ec;

    listen_endpoint = std::move(endpoints->endpoint());
    acceptor.open(listen_endpoint.protocol());
    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor.bind(listen_endpoint);
    acceptor.listen();
    co_return success();
  }

  auto accept() -> boost::asio::awaitable<Result<std::shared_ptr<TcpConn>>> {
    if (!acceptor.is_open()) {
      co_return Error::format("acceptor closed");
    }

    auto [ec, socket] = co_await acceptor.async_accept(boost::asio::experimental::as_tuple(boost::asio::use_awaitable));
    if (ec)
      co_return ec;

    auto conn = TcpConn::create(std::move(socket));
    co_return conn;
  }

  boost::asio::strand<boost::asio::any_io_executor> strand;

private:
  boost::asio::ip::tcp::endpoint listen_endpoint;
  boost::asio::ip::tcp::acceptor acceptor;
};

template<typename T>
auto new_tcp_listener(T& ex) {
  if constexpr (ExecutionContext<T>) {
    auto executor = ex.get_executor();
    return TcpListener::create(executor);
  } else {
    return TcpListener::create(ex);
  }
}

auto new_tcp_listener() {
  return new_tcp_listener(eo::runtime::execution_context);
}

} // namespace noir::net
