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
#include <mutex>

namespace noir {

class Timer {
public:
  Timer(boost::asio::io_context& io_context, std::chrono::seconds dur)
    : io_context(io_context), timer(io_context), dur(dur), trigger_ch(io_context), stop_ch(io_context) {
    timer_routine();
  }

  auto set_func(std::function<boost::asio::awaitable<Result<void>>()>&& f) {
    this->f = f;
  }

  auto after_func(std::function<boost::asio::awaitable<Result<void>>()>&& f) {
    this->f = f;
    trigger_func();
  }

  auto reset(std::chrono::seconds dur) {
    if (!f)
      throw std::runtime_error("time: Reset called on uninitialized Timer");
    this->dur = dur;
    trigger_func();
  }

  void stop() {
    boost::asio::co_spawn(
      io_context,
      [&]() -> boost::asio::awaitable<void> {
        boost::system::error_code ec{};
        co_await stop_ch.async_send(ec, {}, boost::asio::use_awaitable);
      },
      boost::asio::detached);
  }

private:
  void timer_routine() {
    boost::asio::co_spawn(
      io_context,
      [&]() -> boost::asio::awaitable<void> {
        for (;;) {
          co_await trigger_ch.async_receive(boost::asio::use_awaitable);
          timer.expires_after(dur);
          co_await timer.async_wait(boost::asio::use_awaitable);
          auto res = co_await (stop_ch.async_receive(as_result(boost::asio::use_awaitable)) || this->f());
          if (res.index() == 0)
            co_return;
        }
      },
      boost::asio::detached);
  }

  void trigger_func() {
    boost::asio::co_spawn(
      io_context,
      [&]() -> boost::asio::awaitable<void> {
        boost::system::error_code ec{};
        co_await trigger_ch.async_send(ec, {}, boost::asio::use_awaitable);
      },
      boost::asio::detached);
  }

private:
  boost::asio::io_context& io_context;
  boost::asio::steady_timer timer;
  std::chrono::seconds dur;
  std::function<boost::asio::awaitable<Result<void>>()> f;
  Chan<Done> trigger_ch;
  Chan<Done> stop_ch;
  std::mutex mtx;
};
} //namespace noir
