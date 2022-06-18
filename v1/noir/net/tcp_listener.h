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
  explicit TcpListener(boost::asio::io_context& io_context): strand(io_context.get_executor()), acceptor(io_context) {}

public:
  [[nodiscard]] static auto create(boost::asio::io_context& io_context) -> std::shared_ptr<TcpListener> {
    return std::shared_ptr<TcpListener>(new TcpListener(io_context));
  }

  auto listen(std::string_view address) -> boost::asio::awaitable<Result<void>> {
    auto colon = address.find(':');
    if (colon == std::string::npos) {
      co_return Error::format("failed to parse address: {}", address);
    }
    auto host = address.substr(0, colon);
    auto port = address.substr(colon + 1);

    std::shared_ptr<boost::asio::ip::tcp::resolver> resolver = std::make_shared<boost::asio::ip::tcp::resolver>(strand);
    auto res = co_await resolver->async_resolve(boost::asio::ip::tcp::v4(), host, port, as_result(boost::asio::use_awaitable));
    if (res.has_error())
      co_return res.error();

    auto endpoints = res.value();
    listen_endpoint = std::move(endpoints->endpoint());
    acceptor.open(listen_endpoint.protocol());
    acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor.bind(listen_endpoint);
    acceptor.listen();
    co_return success();
  }

  auto accept() -> boost::asio::awaitable<Result<std::shared_ptr<TcpConn>>> {
    if (!acceptor.is_open()) {
      co_return Error("acceptor closed");
    }

    auto [ec, socket] = co_await acceptor.async_accept(boost::asio::experimental::as_tuple(boost::asio::use_awaitable));
    if (ec)
      co_return ec;

    auto conn = TcpConn::create(std::move(socket));
    co_return conn;
  }

  auto close() -> Result<void> {
    boost::system::error_code ec{};
    acceptor.close(ec);
    if (ec) {
      return ec;
    }
    return success();
  }

  boost::asio::strand<boost::asio::io_context::executor_type> strand;

private:
  boost::asio::ip::tcp::endpoint listen_endpoint;
  boost::asio::ip::tcp::acceptor acceptor;
};

} //namespace noir::net
