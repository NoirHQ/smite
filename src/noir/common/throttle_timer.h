// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/timer.h>
#include <noir/core/core.h>
#include <noir/core/error.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <eo/core.h>

namespace noir {

class ThrottleTimer {
public:
  ThrottleTimer(boost::asio::io_context& io_context, std::chrono::milliseconds dur)
    : io_context(io_context), event_ch(io_context), quit_ch(io_context), dur(dur), timer(io_context, dur) {
    timer.set_func([&]() -> boost::asio::awaitable<Result<void>> {
      boost::system::error_code ec{};
      auto res = co_await (quit_ch.async_receive(as_result(boost::asio::use_awaitable)) ||
        event_ch.async_send(ec, {}, boost::asio::use_awaitable));
      if (res.index() == 0) {
        co_return std::get<0>(res).error();
      } else {
        is_set = false;
        co_return success();
      }
    });
  }

  void set() {
    if (!is_set) {
      is_set = true;
      timer.reset(dur);
    }
  }

  void stop() {
    quit_ch.close();
    timer.stop();
  }

public:
  std::string name;
  eo::chan<std::monostate> event_ch;

private:
  boost::asio::io_context& io_context;

  eo::chan<std::monostate> quit_ch;
  std::chrono::milliseconds dur;
  Timer timer;
  bool is_set{false};
};
} // namespace noir