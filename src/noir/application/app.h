// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/abci_types.h>
#include <tendermint/abci/types.pb.h>

namespace noir::application {

using namespace tendermint::abci;

class base_application {
protected:
  // TODO : these variables are temporary. need to change to queue and need to be moved to another class.
  consensus::response_prepare_proposal response_prepare_proposal_;
  consensus::response_commit response_commit_;

public:
  base_application() {}

  virtual std::unique_ptr<ResponseInfo> info_sync(const RequestInfo& req) {
    return {};
  }

  virtual std::unique_ptr<ResponseInitChain> init_chain(const RequestInitChain& req) {
    return {};
  }

  virtual std::unique_ptr<ResponseBeginBlock> begin_block(const RequestBeginBlock& req) {
    return {};
  }
  virtual std::unique_ptr<ResponseEndBlock> end_block(const RequestEndBlock& req) {
    return {};
  }
  virtual std::unique_ptr<ResponseDeliverTx> deliver_tx_async(const RequestDeliverTx& req) {
    return {};
  }
  virtual std::unique_ptr<ResponseCommit> commit() {
    return std::make_unique<ResponseCommit>();
  }

  virtual std::unique_ptr<ResponseCheckTx> check_tx_sync() {
    return {};
  }
  virtual std::unique_ptr<ResponseCheckTx> check_tx_async() {
    return {};
  }

  virtual consensus::response_prepare_proposal& prepare_proposal() {
    return response_prepare_proposal_;
  }

  virtual void list_snapshots() {}
  virtual void offer_snapshot() {}
  virtual void load_snapshot_chunk() {}
  virtual void apply_snapshot_chunk() {}
};

} // namespace noir::application
