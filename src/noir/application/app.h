// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/abci_types.h>

namespace noir::application {

class app {
public:
  void info();
  void query();

  void check_tx();

  consensus::response_init_chain init_chain() {
    return consensus::response_init_chain{};
  }
  consensus::response_prepare_proposal prepare_proposal() {
    return consensus::response_prepare_proposal{};
  }

  consensus::response_begin_block begin_block() {
    return consensus::response_begin_block{};
  }
  consensus::response_deliver_tx deliver_tx() {
    return consensus::response_deliver_tx{};
  };
  consensus::response_end_block end_block() {
    return consensus::response_end_block{};
  }
  consensus::response_commit commit() {
    return consensus::response_commit{};
  }
  consensus::response_extend_vote extend_vote() {
    return consensus::response_extend_vote{};
  }
  consensus::response_verify_vote_extension verify_vote_extension() {
    return consensus::response_verify_vote_extension{};
  }

  void list_snapshots();
  void offer_snapshot();
  void load_snapshot_chunk();
  void apply_snapshot_chunk();
};

} // namespace noir::application
