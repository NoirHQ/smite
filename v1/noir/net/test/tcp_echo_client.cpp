// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/net/tcp_conn.h>
#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <iostream>

using namespace noir;
using namespace noir::net;

void print_error(std::exception_ptr eptr) {
  try {
    if (eptr) {
      std::rethrow_exception(eptr);
      return;
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }
}

boost::asio::awaitable<void> heartbeat(std::shared_ptr<TcpConn> conn) {
  auto msg = std::string{"Listen to my heartbeat"};
  auto send_buffer = boost::asio::buffer((const unsigned char*)msg.data(), msg.size());
  auto recv_buffer = std::array<unsigned char, 256>{};
  auto timer = boost::asio::steady_timer{conn->strand};

  for (;;) {
    if (auto ok = co_await conn->write(send_buffer); !ok) {
      std::cerr << ok.error().message() << std::endl;
    }
    auto ok = co_await conn->read({recv_buffer.data(), msg.size()});
    if (!ok) {
      std::cerr << ok.error().message() << std::endl;
    } else {
      std::string_view str{(const char*)recv_buffer.data(), msg.size()};
      std::cout << str << std::endl;
    }
    std::fill(recv_buffer.begin(), recv_buffer.end(), 0);
    timer.expires_after(std::chrono::milliseconds(1000));
    co_await timer.async_wait(boost::asio::use_awaitable);
  }
  co_return;
}

int main() {
  boost::asio::io_context io_context{};

  auto conn = TcpConn::create("localhost:26658", io_context);

  boost::asio::co_spawn(
    conn->strand,
    [conn, &io_context]() -> boost::asio::awaitable<void> {
      if (auto ok = co_await conn->connect(); !ok) {
        std::cerr << ok.error().message() << std::endl;
        co_return;
      }
      co_await heartbeat(conn);
    },
    print_error);

  auto work = boost::asio::make_work_guard(io_context);
  (void)work;

  io_context.run();
}
