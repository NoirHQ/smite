// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/ticker.h>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>

using namespace noir;

int main() {
  boost::asio::io_context io_context{};
  Ticker t(io_context, std::chrono::seconds{1});
  t.start();

  boost::asio::co_spawn(
    io_context,
    [&]() -> boost::asio::awaitable<void> {
      for (;;) {
        auto res = co_await t.time_ch.async_receive(boost::asio::use_awaitable);
        std::cout << "time: " << res.time_since_epoch().count() << std::endl;
      }
    },
    boost::asio::detached);
  io_context.run();
}