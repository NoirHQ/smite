// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/helper/variant.h>
#include <noir/common/refl.h>
#include <tendermint/abci/types.pb.h>

namespace noir::tendermint::rpc {

struct result_broadcast_tx {
  uint32_t code;
  Bytes data;
  std::string log;
  std::string codespace;
  std::string mempool_error;

  Bytes32 hash;
};

struct result_broadcast_tx_commit {
  ::tendermint::abci::ResponseCheckTx check_tx;
  ::tendermint::abci::ResponseDeliverTx deliver_tx;
  Bytes hash;
  int64_t height;
};

struct result_unconfirmed_txs {
  uint64_t count;
  uint64_t total;
  uint64_t total_bytes;
  std::vector<std::shared_ptr<const Bytes>> txs;
};

} // namespace noir::tendermint::rpc

NOIR_REFLECT(tendermint::rpc::result_broadcast_tx, code, data, log, codespace, mempool_error, hash);
NOIR_REFLECT(tendermint::rpc::result_unconfirmed_txs, count, total, total_bytes, txs);
