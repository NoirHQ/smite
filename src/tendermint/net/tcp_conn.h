// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <tendermint/net/conn.h>
#include <tendermint/net/details/log.h>
#include <tendermint/net/details/verify_strand.h>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

namespace tendermint::net {

class TCPConn : public Conn<TCPConn> {
public:
  TCPConn(const std::string& address, boost::asio::io_context& io_context)
    : Conn(address, io_context), socket(new boost::asio::ip::tcp::socket(io_context.get_executor())) {}

  result<void> on_connect() {
    auto colon = address.find(':');
    if (colon == std::string::npos) {
      return make_unexpected("invalid address");
    }
    auto host = address.substr(0, colon);
    auto port = address.substr(colon + 1);

    boost::system::error_code ec;
    boost::asio::ip::tcp::resolver resolver(strand.context());
    auto endpoints = resolver.resolve(boost::asio::ip::tcp::v4(), host, port, ec);
    if (ec) {
      return make_unexpected(ec.message());
    }
    boost::asio::connect(*socket, endpoints, ec);
    if (ec) {
      return make_unexpected(ec.message());
    }
    return {};
  }

  std::future<result<void>> on_async_connect(ConnectHandler&& ch) {
    std::promise<result<void>> promise{};
    auto future = promise.get_future();
    auto conn = shared_from_this();

    boost::asio::post(strand, [conn, promise{std::move(promise)}, ch{std::move(ch)}]() mutable {
      const auto& address = conn->address;
      auto colon = address.find(':');
      if (colon == std::string::npos) {
        promise.set_value(make_unexpected("invalid address"));
        return;
      }
      auto host = address.substr(0, colon);
      auto port = address.substr(colon + 1);

      auto resolver = std::make_shared<boost::asio::ip::tcp::resolver>(conn->strand.context());
      auto weak_conn = std::weak_ptr(conn);
      resolver->async_resolve(boost::asio::ip::tcp::v4(), host, port,
        boost::asio::bind_executor(conn->strand,
          [resolver, weak_conn, host, port, promise{std::move(promise)}, ch{std::move(ch)}](
            const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type endpoints) mutable {
            auto conn = weak_conn.lock();
            if (!conn) {
              promise.set_value(make_unexpected("freed connection"));
              return;
            }
            if (ec) {
              promise.set_value(make_unexpected(ec.message()));
              return;
            }
            boost::asio::async_connect(*static_cast<TCPConn*>(conn.get())->socket, endpoints,
              boost::asio::bind_executor(static_cast<TCPConn*>(conn.get())->strand,
                [conn, promise{std::move(promise)}, ch{std::move(ch)}](
                  const boost::system::error_code& ec, const boost::asio::ip::tcp::endpoint& endpoint) mutable {
                  if (ec) {
                    promise.set_value(make_unexpected(ec.message()));
                  }
                  if (ch) {
                    ch(ec);
                  }
                  promise.set_value({});
                }));
          }));
    });
    return future;
  }

public:
  std::shared_ptr<boost::asio::ip::tcp::socket> socket;
};

} // namespace tendermint::net
