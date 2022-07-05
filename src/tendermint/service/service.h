// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/log/log.h>
#include <eo/core.h>
#include <atomic>

namespace noir::service {

extern const Error err_already_started;
extern const Error err_already_stopped;
extern const Error err_not_started;

template<typename T>
concept Service = requires(T v) {
  { v.start() } -> std::same_as<Result<void>>;
  { v.on_start() } -> std::same_as<Result<void>>;
  { v.stop() } -> std::same_as<Result<void>>;
  v.on_stop();
  { v.reset() } -> std::same_as<Result<void>>;
  { v.on_reset() } -> std::same_as<Result<void>>;
  { v.is_running() } -> std::same_as<bool>;
  { v.quit() } -> std::same_as<eo::chan<>>;
  { v.to_string() } -> std::same_as<const std::string&>;
  v.set_logger(std::make_shared<log::Logger>());
  v.wait();
};

// XXX: optimize atomic operations in memory order - relaxed?
template<typename Derived>
class BaseService {
public:
  Result<void> start() {
    bool started_expected = false;
    if (started.compare_exchange_strong(started_expected, true)) {
      if (stopped.load()) {
        noir_elog(logger.get(), "Not starting {} service -- already stopped", name);
        started.store(false);
        return err_already_stopped;
      }
      noir_ilog(logger.get(), "Starting {} service", name);
      auto res = static_cast<Derived*>(this)->on_start();
      if (!res) {
        started.store(false);
        return res.error();
      }
      return success();
    }
    noir_ilog(logger.get(), "Not starting {} service -- already started", name);
    return err_already_started;
  }

  Result<void> stop() {
    bool stopped_expected = false;
    if (stopped.compare_exchange_strong(stopped_expected, true)) {
      if (!started.load()) {
        noir_elog(logger.get(), "Not stopping {} service -- has not been started yet", name);
        stopped.store(false);
        return err_not_started;
      }
      noir_ilog(logger.get(), "Stopping {} service", name);
      static_cast<Derived*>(this)->on_stop();
      // quit
      return success();
    }
    noir_ilog(logger.get(), "Stopping {} service (already stopped)", name);
    return err_already_stopped;
  }

  Result<void> reset() {
    bool stopped_expected = true;
    if (!stopped.compare_exchange_strong(stopped_expected, false)) {
      noir_ilog(logger.get(), "Can't reset {} service, Not stopped", name);
      return Error::format("can't reset running {}", name);
    }

    bool started_expected = true;
    started.compare_exchange_strong(started_expected, false);
    return static_cast<Derived*>(this)->on_reset();
  }

  bool is_running() const {
    return started.load() && !stopped.load();
  }

  const std::string& to_string() const {
    return name;
  }

  void set_logger(std::shared_ptr<log::Logger> l) {
    this->logger = l;
  }

  // wait

  auto quit() -> eo::chan<>& {
    return quit_;
  }

protected:
  std::string name;
  std::shared_ptr<log::Logger> logger = log::default_logger();
  eo::chan<> quit_ = eo::make_chan();

private:
  std::atomic_bool started;
  std::atomic_bool stopped;
  // quit
};

} // namespace noir::service
