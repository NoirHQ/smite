// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/throttle_timer.h>
#include <boost/asio/io_context.hpp>
#include <iostream>

using namespace noir;

int main() {
  boost::asio::io_context io_context{};
  ThrottleTimer tt{io_context, std::chrono::milliseconds{1000}};
  tt.set();

  boost::asio::co_spawn(
    io_context,
    [&io_context, &tt]() -> boost::asio::awaitable<void> {
      for (;;) {
        co_await tt.event_ch.async_receive(boost::asio::use_awaitable);

        boost::asio::steady_timer t{io_context};
        t.expires_after(std::chrono::milliseconds{1000});
        co_await t.async_wait(boost::asio::use_awaitable);
        tt.set();
      }
    },
    boost::asio::detached);
  io_context.run();
}