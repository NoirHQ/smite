// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/application/app.h>

namespace noir::application {

class noop_app : public base_application {

  virtual consensus::response_begin_block& begin_block() override {
    ilog("!!! BeginBlock !!!");
    return response_begin_block_;
  }
  virtual consensus::req_res<consensus::response_deliver_tx>& deliver_tx_async() override {
    ilog("!!! DeliverTx !!!");
    return req_res_deliver_tx_;
  }
  virtual consensus::response_end_block& end_block() override {
    ilog("!!! EndBlock !!!");
    return response_end_block_;
  }
};

} // namespace noir::application
