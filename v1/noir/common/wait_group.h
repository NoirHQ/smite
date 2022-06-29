// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <boost/asio/io_context.hpp>
#include <atomic>

namespace noir {
class WaitGroup {
public:
  WaitGroup(boost::asio::io_context& io_context): sync(io_context){};

  void add() {
    index++;
  }

  auto done() -> boost::asio::awaitable<void> {
    index--;
    if (index == 0) {
      co_await sync.async_send(boost::system::error_code{}, {}, boost::asio::use_awaitable);
    }
  }

  auto wait() -> boost::asio::awaitable<void> {
    co_await sync.async_receive(boost::asio::use_awaitable);
  }

private:
  std::atomic_int index{0};
  boost::asio::experimental::concurrent_channel<std::monostate(boost::system::error_code, std::monostate)> sync;
};
} //namespace noir