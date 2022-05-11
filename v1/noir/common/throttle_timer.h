// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/timer.h>
#include <noir/core/core.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <mutex>

namespace noir {

class ThrottleTimer {
public:
  ThrottleTimer(boost::asio::io_context& io_context, std::chrono::seconds dur)
    : io_context(io_context), event_ch(io_context), quit_ch(io_context), dur(dur), timer(io_context, dur) {
    std::scoped_lock _{mtx};
    timer.set_func(fire_routine());
  }

  void set() {
    std::scoped_lock _{mtx};
    if (!is_set) {
      is_set = true;
      timer.reset(dur);
    }
  }

public:
  std::string name;
  Chan<noir::Done> event_ch;

private:
  auto fire_routine() -> std::function<boost::asio::awaitable<Result<void>>()> {
    return [&]() -> boost::asio::awaitable<Result<void>> {
      std::scoped_lock _{mtx};

      boost::asio::steady_timer t{io_context};
      t.expires_after(std::chrono::microseconds{100});
      auto res = co_await (event_ch.async_send(boost::system::error_code{}, {}, boost::asio::use_awaitable) ||
        quit_ch.async_receive(as_result(boost::asio::use_awaitable)) || t.async_wait(boost::asio::use_awaitable));
      switch (res.index()) {
      case 0:
        is_set = false;
        // TODO: how to handle ec?
        co_return success();
      case 1:
        co_return std::get<1>(res).error();
      case 2:
        timer.reset(dur);
        co_return success();
      }
      co_return err_unreachable;
    };
  }

private:
  boost::asio::io_context& io_context;

  Chan<noir::Done> quit_ch;
  std::chrono::seconds dur;
  Timer timer;
  std::mutex mtx;
  bool is_set{false};
};
} //namespace noir