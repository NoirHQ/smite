// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/application/app.h>
#include <noir/consensus/abci_types.h>
#include <noir/consensus/types.h>

namespace noir::consensus {

struct app_connection {
  std::mutex mtx;

  std::shared_ptr<application::base_application> application;

  // set_response_callback(callback) // todo - skip for now; maybe not needed for noir

  response_init_chain& init_chain_sync(request_init_chain req) {
    std::lock_guard<std::mutex> g(mtx);
    auto& res = application->init_chain();
    return res;
  }

  response_prepare_proposal& prepare_proposal_sync(request_prepare_proposal req) {
    std::lock_guard<std::mutex> g(mtx);
    auto& res = application->prepare_proposal();
    return res;
  }

  response_begin_block& begin_block_sync(requst_begin_block req) {
    std::lock_guard<std::mutex> g(mtx);
    auto& res = application->begin_block();
    return res;
  }

  response_deliver_tx& deliver_tx_async(request_deliver_tx req) {
    std::lock_guard<std::mutex> g(mtx);
    auto& res = application->deliver_tx();
    return res;
  }

  response_check_tx& check_tx_async(request_check_tx req) {
    std::lock_guard<std::mutex> g(mtx);
    auto& res = application->check_tx();
    return res;
  }

  response_end_block& end_block_sync(request_end_block req) {
    std::lock_guard<std::mutex> g(mtx);
    auto& res = application->end_block();
    return res;
  }
  response_commit& commit_sync() {
    std::lock_guard<std::mutex> g(mtx);
    auto& res = application->commit();
    return res;
  }

  response_extend_vote& extend_vote_sync(request_extend_vote req) {
    std::lock_guard<std::mutex> g(mtx);
    auto& res = application->extend_vote();
    return res;
  }
  response_verify_vote_extension& verify_vote_extension_sync(request_verify_vote_extension req) {
    std::lock_guard<std::mutex> g(mtx);
    auto& res = application->verify_vote_extension();
    return res;
  }

  void flush_async() {
    std::lock_guard<std::mutex> g(mtx);
  }
  void flush_sync() {
    std::lock_guard<std::mutex> g(mtx);
  }

  app_connection() {
    application = std::make_shared<application::app>();
  }
};

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::app_connection, mtx, application);
