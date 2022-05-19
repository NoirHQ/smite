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
  boost::asio::io_context io_context;
  Timer timer{io_context, std::chrono::milliseconds{1000}};

  timer.set_func([&]() -> boost::asio::awaitable<Result<void>> {
    std::cout << "function start" << std::endl;
    boost::asio::steady_timer st{io_context};
    st.expires_after(std::chrono::milliseconds{1000});

    co_await st.async_wait(boost::asio::use_awaitable);
    std::cout << "function end" << std::endl;
    timer.reset(std::chrono::milliseconds{1000});
    co_return success();
  });
  timer.reset(std::chrono::milliseconds{1000});

  boost::asio::co_spawn(
    io_context,
    [&]() -> boost::asio::awaitable<void> {
      boost::asio::steady_timer dt{io_context};
      dt.expires_after(std::chrono::milliseconds{5000});
      co_await dt.async_wait(boost::asio::use_awaitable);
      timer.stop();
    },
    boost::asio::detached);

  io_context.run();
}