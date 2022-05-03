// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/net/tcp_listener.h>
#include <boost/asio/co_spawn.hpp>
#include <iostream>
#include <thread>

using namespace noir;
using namespace noir::net;

void print_error(const std::exception_ptr& eptr) {
  try {
    if (eptr) {
      std::rethrow_exception(eptr);
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

const std::string ping = "ping!";
const std::string pong = "pong!";

boost::asio::awaitable<void> send_routine(std::shared_ptr<TcpConn> conn) {
  auto send_buffer = boost::asio::buffer((const unsigned char*)ping.data(), ping.size());
  auto recv_buffer = std::array<unsigned char, 256>{};
  auto timer = boost::asio::steady_timer{conn->strand};

  for (;;) {
    if (auto ok = co_await conn->write(send_buffer); !ok) {
      std::cerr << ok.error().message() << std::endl;
    } else {
      std::string_view str{(const char*)send_buffer.data(), ping.size()};
      std::cout << "Client Send: " << str << std::endl;

      auto ok2 = co_await conn->read({recv_buffer.data(), pong.size()});
      if (!ok2) {
        std::cerr << ok2.error().message() << std::endl;
      } else {
        std::string_view str2{(const char*)recv_buffer.data(), pong.size()};
        std::cout << "Client Receive: " << str2 << std::endl;
      }

      std::fill(recv_buffer.begin(), recv_buffer.end(), 0);
    }

    timer.expires_after(std::chrono::milliseconds(1000));
    co_await timer.async_wait(boost::asio::use_awaitable);
  }
}

boost::asio::awaitable<void> receive_routine(std::shared_ptr<TcpConn> conn) {
  auto send_buffer = boost::asio::buffer((const unsigned char*)pong.data(), pong.size());
  auto recv_buffer = std::array<unsigned char, 256>{};

  for (;;) {
    auto ok = co_await conn->read({recv_buffer.data(), ping.size()});
    if (!ok) {
      std::cerr << ok.error().message() << std::endl;
    } else {
      if (auto ok2 = co_await conn->write(send_buffer); !ok2) {
        std::cerr << ok2.error().message() << std::endl;
      }
    }
    std::fill(recv_buffer.begin(), recv_buffer.end(), 0);
  }
}

int main() {
  std::thread t1([]() {
    boost::asio::io_context io_context{};

    auto listener = TcpListener::create(io_context);
    boost::asio::co_spawn(
      io_context,
      [listener]() -> boost::asio::awaitable<void> {
        if (auto ok = co_await listener->listen("127.0.0.1:26658"); !ok) {
          std::cerr << ok.error().message() << std::endl;
          co_return;
        }
        Result<std::shared_ptr<TcpConn>> result = co_await listener->accept();
        auto conn = result.value();
        auto executor = co_await boost::asio::this_coro::executor;
        boost::asio::co_spawn(executor, receive_routine(conn), print_error);
      },
      print_error);
    io_context.run();
  });

  boost::asio::io_context io_context{};
  auto conn = TcpConn::create("127.0.0.1:26658", io_context);
  boost::asio::co_spawn(
    io_context,
    [conn]() -> boost::asio::awaitable<void> {
      if (auto ok = co_await conn->connect(); !ok) {
        std::cerr << ok.error().message() << std::endl;
        co_return;
      }
      auto executor = co_await boost::asio::this_coro::executor;
      boost::asio::co_spawn(executor, send_routine(conn), print_error);
    },
    print_error);

  io_context.run();
  t1.join();
}
