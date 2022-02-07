// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/abci_types.h>
#include <noir/consensus/types.h>

namespace noir::consensus {

struct app_connection {
  // set_response_callback(callback) // todo - skip for now; maybe not needed for noir

  response_init_chain init_chain_sync(request_init_chain req) {}

  response_prepare_proposal prepare_proposal_sync(request_prepare_proposal req) {}

  response_begin_block begin_block_sync(requst_begin_block req) {}
  // deliver_tx_async // todo
  response_end_block end_block_sync(request_end_block req) {}
  response_commit commit_sync() {}

  response_extend_vote extend_vote_sync(request_extend_vote req) {}
  response_verify_vote_extension verify_vote_extension_sync(request_verify_vote_extension req) {}
};

} // namespace noir::consensus
