// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/abci_types.h>

namespace noir::application {

class base_application {
public:
  virtual void info() {}
  virtual void query() {}

  virtual consensus::response_init_chain init_chain() {
    return consensus::response_init_chain{};
  }
  virtual consensus::response_prepare_proposal prepare_proposal() {
    return consensus::response_prepare_proposal{};
  }

  virtual consensus::response_begin_block begin_block() {
    ilog("!!! BeginBlock !!!");
    return consensus::response_begin_block{};
  }
  virtual consensus::response_deliver_tx deliver_tx() {
    ilog("!!! DeliverTx !!!");
    return consensus::response_deliver_tx{};
  }
  virtual consensus::response_check_tx check_tx() {
    return consensus::response_check_tx{};
  }
  virtual consensus::response_end_block end_block() {
    ilog("!!! EndBlock !!!");
    return consensus::response_end_block{};
  }
  virtual consensus::response_commit commit() {
    return consensus::response_commit{};
  }
  virtual consensus::response_extend_vote extend_vote() {
    return consensus::response_extend_vote{};
  }
  virtual consensus::response_verify_vote_extension verify_vote_extension() {
    return consensus::response_verify_vote_extension{};
  }

  virtual void list_snapshots() {}
  virtual void offer_snapshot() {}
  virtual void load_snapshot_chunk() {}
  virtual void apply_snapshot_chunk() {}
};

class app : public base_application {};

} // namespace noir::application
