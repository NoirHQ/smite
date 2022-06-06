// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/core/core.h>

#include <boost/asio/awaitable.hpp>
#include <boost/asio/steady_timer.hpp>

#include <chrono>

using namespace noir;
using namespace boost::asio::experimental::awaitable_operators;

boost::asio::awaitable<void> recv(Chan<Done>& done, Chan<int>& value) {
  for (;;) {
    auto res = co_await(
      done.async_receive(as_result(boost::asio::use_awaitable)) || value.async_receive(boost::asio::use_awaitable));
    switch (res.index()) {
    case 0:
      co_return;
    case 1:
      std::cout << std::get<1>(res) << std::endl;
    }
  }
}

boost::asio::awaitable<void> send(Chan<Done>& done, Chan<int>& value, boost::asio::steady_timer&& timer) {
  boost::system::error_code ec;
  for (auto i = 1; i <= 5; ++i) {
    co_await value.async_send(ec, i, boost::asio::use_awaitable);
    timer.expires_after(std::chrono::seconds(1));
    co_await timer.async_wait(boost::asio::use_awaitable);
  }
  done.close();
}

TEST_CASE("clist: channel", "[noir][clist]") {
  boost::asio::io_context io_context{};

  Chan<Done> done{io_context};
  Chan<int> value{io_context};

  boost::asio::co_spawn(io_context, recv(done, value), boost::asio::detached);
  boost::asio::co_spawn(io_context, send(done, value, boost::asio::steady_timer{io_context}), boost::asio::detached);

  io_context.run();
}
