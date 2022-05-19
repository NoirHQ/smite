// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/core.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

namespace noir {

class Timer {
public:
  Timer(boost::asio::io_context& io_context, std::chrono::milliseconds dur)
    : io_context(io_context), timer(io_context), d(dur), trigger_ch(io_context), stop_ch(io_context) {
    timer_routine();
  }

  void set_func(std::function<boost::asio::awaitable<Result<void>>()>&& func) {
    f = func;
  }

  void reset(std::chrono::milliseconds dur) {
    if (!f) {
      throw std::runtime_error("time: Reset called on uninitialized Timer");
    }
    d = dur;
    boost::asio::co_spawn(
      io_context,
      [&]() -> boost::asio::awaitable<void> {
        boost::system::error_code ec{};
        co_await trigger_ch.async_send(ec, {}, boost::asio::use_awaitable);
      },
      boost::asio::detached);
  }

  void stop() {
    stop_ch.close();
  }

private:
  void timer_routine() {
    boost::asio::co_spawn(
      io_context,
      [&]() -> boost::asio::awaitable<void> {
        for (;;) {
          co_await trigger_ch.async_receive(boost::asio::use_awaitable);
          timer.expires_after(d);
          co_await timer.async_wait(boost::asio::use_awaitable);
          auto res = co_await (stop_ch.async_receive(as_result(boost::asio::use_awaitable)) || f());
          if (res.index() == 0)
            co_return;
        }
      },
      boost::asio::detached);
  }

private:
  boost::asio::io_context& io_context;
  boost::asio::steady_timer timer;
  std::chrono::milliseconds d;
  std::function<boost::asio::awaitable<Result<void>>()> f;
  Chan<Done> trigger_ch;
  Chan<Done> stop_ch;
};
} //namespace noir
