// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/config/mempool.h>

namespace noir::config {

void MempoolConfig::bind(CLI::App& root) {
  auto mempool = root.add_section("mempool", R"(
######################################################
###          Mempool Configuration Option          ###
######################################################)");

  mempool->add_option("--version", version, R"(
# Mempool version to use:
#   1) "v0" - The legacy non-prioritized mempool reactor.
#   2) "v1" (default) - The prioritized mempool reactor.)");
  mempool->add_option("--recheck", recheck);
  mempool->add_option("--broadcast", broadcast);
  mempool->add_option("--size", size, "Maximum number of transactions in the mempool");
  mempool->add_option("--max-txs-bytes", max_txs_bytes, R"(
# Limit the total size of all txs in the mempool.
# This only accounts for raw transactions (e.g. given 1MB transactions and
# max-txs-bytes=5MB, mempool will only accept 5 transactions).)");
  mempool->add_option("--cache-size", cache_size, "Size of the cache (used to filter transactions we saw earlier) in transactions");
  mempool->add_option("--keep-invalid-txs-in-chache", keep_invalid_txs_in_cache, R"(
# Do not remove invalid transactions from the cache (default: false)
# Set to true if it's not possible for any invalid transaction to become valid
# again in the future.)");
  mempool->add_option("--max-tx-bytes", max_tx_bytes, R"(
# Maximum size of a single transaction.
# NOTE: the max size of a tx transmitted over the network is {max-tx-bytes}.)");
  mempool->add_option("--max-batch-bytes", max_batch_bytes, R"(
# Maximum size of a batch of transactions to send to a peer
# Including space needed by encoding (one varint per transaction).
# XXX: Unused due to https://github.com/tendermint/tendermint/issues/5796)");
  mempool->add_option("--ttl-duration", ttl_duration, R"(
# ttl-duration, if non-zero, defines the maximum amount of time a transaction
# can exist for in the mempool.
#
# Note, if ttl-num-blocks is also defined, a transaction will be removed if it
# has existed in the mempool at least ttl-num-blocks number of blocks or if it's
# insertion time into the mempool is beyond ttl-duration.)");
  mempool->add_option("--ttl-num-blocks", ttl_num_blocks, R"(
# ttl-num-blocks, if non-zero, defines the maximum number of blocks a transaction
# can exist for in the mempool.
#
# Note, if ttl-duration is also defined, a transaction will be removed if it
# has existed in the mempool at least ttl-num-blocks number of blocks or if
# it's insertion time into the mempool is beyond ttl-duration.)");
}

} // namespace noir::config
