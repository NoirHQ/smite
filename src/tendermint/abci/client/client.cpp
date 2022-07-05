// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/abci/client/client.h>

namespace noir::abci {

void ReqRes::set_callback(std::function<void(Response*)> cb) {
  std::unique_lock _{mtx};

  if (callback_invoked) {
    _.unlock();
    cb(response.get());
    return;
  }

  this->cb = cb;
}

void ReqRes::invoke_callback() {
  std::unique_lock _{mtx};

  if (cb) {
    cb(response.get());
  }
  callback_invoked = true;
}

auto ReqRes::get_callback() -> std::function<void(Response*)>& {
  std::unique_lock _{mtx};
  return cb;
}

void ReqRes::add(int delta) {
  wg.add(delta);
}

void ReqRes::done() {
  wg.done();
}

void ReqRes::wait() {
  wg.wait();
}

} // namespace noir::abci
