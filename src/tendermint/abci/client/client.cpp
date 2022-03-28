// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <tendermint/abci/client/client.h>

namespace tendermint::abci {

void ReqRes::set_callback(std::function<void(Response*)> cb) {
  std::scoped_lock _(mtx);
  if (done) {
    cb(response);
    return;
  }
  this->cb = cb;
}

void ReqRes::invoke_callback(Response* r) {
  std::scoped_lock _(mtx);
  if (cb) {
    cb(response);
  }
}

std::function<void(Response*)> ReqRes::get_callback() {
  std::scoped_lock _(mtx);
  return cb;
}

void ReqRes::set_done() {
  std::scoped_lock _(mtx);
  done = true;
}

} // namespace tendermint::abci
