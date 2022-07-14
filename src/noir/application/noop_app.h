// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/application/app.h>
//#include <noir/common/log.h>

namespace noir::application {

class noop_app : public base_application {
public:
  noop_app() {}

  virtual std::unique_ptr<ResponseBeginBlock> begin_block(const RequestBeginBlock& req) override {
    // ilog("!!! BeginBlock !!!");
    return {};
  }
  virtual std::unique_ptr<ResponseEndBlock> end_block(const RequestEndBlock& req) override {
    // ilog("!!! EndBlock !!!");
    return {};
  }
  virtual std::unique_ptr<ResponseDeliverTx> deliver_tx_async(const RequestDeliverTx& req) override {
    // ilog("!!! DeliverTx !!!");
    return {};
  }
};

} // namespace noir::application
