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

const std::string msg = std::string{"Hello!"};

boost::asio::awaitable<void> write(std::shared_ptr<TcpConn> conn) {
  auto send_buffer = boost::asio::buffer((const unsigned char*)msg.data(), msg.size());
  auto timer = boost::asio::steady_timer{conn->strand};

  for (;;) {
    std::cout << "Write: " << msg << std::endl;
    if (auto ok = co_await conn->write(send_buffer); !ok) {
      std::cerr << ok.error().message() << std::endl;
    }
    timer.expires_after(std::chrono::milliseconds(1000));
    co_await timer.async_wait(boost::asio::use_awaitable);
  }
}

boost::asio::awaitable<void> read(std::shared_ptr<TcpConn> conn) {
  auto recv_buffer = std::array<unsigned char, 256>{};

  for (;;) {
    auto ok = co_await conn->read({recv_buffer.data(), msg.size()});
    if (!ok) {
      std::cerr << ok.error().message() << std::endl;
    } else {
      std::string_view str{(const char*)recv_buffer.data(), msg.size()};
      std::cout << "Read: " << str << std::endl;
    }
    std::fill(recv_buffer.begin(), recv_buffer.end(), 0);
  }
  co_return;
}

int main() {
  std::thread t1([]() {
    boost::asio::io_context io_context{};

    auto listener = TcpListener::create(io_context);
    boost::asio::co_spawn(
      listener->strand,
      [listener, &io_context]() -> boost::asio::awaitable<void> {
        if (auto ok = co_await listener->listen("127.0.0.1:26658"); !ok) {
          std::cerr << ok.error().message() << std::endl;
          co_return;
        }
        for (;;) {
          Result<std::shared_ptr<TcpConn>> result = co_await listener->accept();
          auto conn = result.value();
          co_await write(conn);
        }
      },
      print_error);
    io_context.run();
  });
  t1.joinable();

  boost::asio::io_context io_context{};
  auto conn = TcpConn::create("127.0.0.1:26658", io_context);
  boost::asio::co_spawn(
    conn->strand,
    [conn, &io_context]() -> boost::asio::awaitable<void> {
      if (auto ok = co_await conn->connect(); !ok) {
        std::cerr << ok.error().message() << std::endl;
        co_return;
      }
      co_await read(conn);
    },
    print_error);
  io_context.run();
}
