// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/application/noop_app.h>
#include <noir/consensus/abci_types.h>
#include <noir/consensus/common.h>

namespace noir::consensus {

struct app_connection {
  std::mutex mtx;

  std::shared_ptr<application::base_application> application;

  response_init_chain& init_chain_sync(request_init_chain req) {
    std::scoped_lock g(mtx);
    auto& res = application->init_chain();
    return res;
  }

  response_prepare_proposal& prepare_proposal_sync(request_prepare_proposal req) {
    std::scoped_lock g(mtx);
    auto& res = application->prepare_proposal();
    return res;
  }

  response_begin_block& begin_block_sync(request_begin_block req) {
    std::scoped_lock g(mtx);
    auto& res = application->begin_block();
    return res;
  }

  auto& deliver_tx_async(request_deliver_tx req) {
    std::scoped_lock g(mtx);
    auto& res = application->deliver_tx_async();
    return res;
  }

  response_check_tx& check_tx_sync(request_check_tx req) {
    std::scoped_lock g(mtx);
    auto& res = application->check_tx_sync();
    return res;
  }

  auto& check_tx_async(request_check_tx req) {
    std::scoped_lock g(mtx);
    auto& res = application->check_tx_async();
    return res;
  }

  response_end_block& end_block_sync(request_end_block req) {
    std::scoped_lock g(mtx);
    auto& res = application->end_block();
    return res;
  }
  response_commit& commit_sync() {
    std::scoped_lock g(mtx);
    auto& res = application->commit();
    return res;
  }

  void flush_async() {
    std::scoped_lock g(mtx);
  }
  void flush_sync() {
    std::scoped_lock g(mtx);
  }

  app_connection(const std::string& proxy_app = "") {
    if (proxy_app == "noop") {
      application = std::make_shared<application::noop_app>();
      return;
    } else if (proxy_app.empty()) {
      application = std::make_shared<application::base_application>();
      return;
    }
    check(false, "failed to load application");
  }
};

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::app_connection, mtx, application);
