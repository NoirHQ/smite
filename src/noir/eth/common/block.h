// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/hex.h>
#include <noir/consensus/types/block.h>
#include <noir/eth/common/transaction.h>
#include <fc/variant_object.hpp>

namespace noir::eth {

struct header {
  Bytes parent_hash;
  Bytes uncle_hash;
  Bytes coinbase;
  Bytes root;
  Bytes tx_hash;
  Bytes receipt_hash;
  Bytes bloom;
  uint256_t difficulty;
  uint256_t number;
  uint64_t gas_limit;
  uint64_t gas_used;
  uint64_t time;
  Bytes extra;
  Bytes mix_digest;
  uint64_t nonce;
  uint256_t base_fee;

public:
  header(const noir::consensus::block_header& h)
    : parent_hash(h.last_commit_hash), coinbase(h.proposer_address), root(h.app_hash), number(h.height), time(h.time) {}

  fc::variant_object to_json() {
    fc::mutable_variant_object h_mvo;
    h_mvo("parentHash", to_hex(parent_hash));
    h_mvo("sha3Uncles", "0000000000000000000000000000000000000000000000000000000000000000");
    h_mvo("miner", to_hex(coinbase));
    h_mvo("stateRoot", to_hex(root));
    // TODO: empty tx root: 56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421?
    h_mvo("transactionsRoot", "56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421");
    h_mvo("receiptsRoot", "56e81f171bcc55a6ff8345e692c0f86e5b48e01b996cadc001622fb5e363b421");
    h_mvo("logsBloom", "");
    h_mvo("difficulty", "0");
    h_mvo("blockNumber", to_hex(number));
    h_mvo("gasLimit", "0");
    h_mvo("gasUsed", "0");
    h_mvo("timestamp", to_hex(time));
    h_mvo("extra", "");
    h_mvo("mixHash", "0000000000000000000000000000000000000000000000000000000000000000");
    h_mvo("nonce", "0");
    h_mvo("baseFeePerGas", "0");
    return h_mvo;
  }
};

struct block {
  eth::header header;
  std::vector<eth::header> uncles;
  std::vector<rpc_transaction> transactions;
  Bytes hash;
  uint64_t size;
  uint256_t td;

public:
  block(noir::consensus::block& b): header(b.header), hash(b.get_hash()) {}

  fc::variant to_json() {
    fc::mutable_variant_object b_mvo;

    b_mvo("header", header.to_json());
    b_mvo("uncles", fc::variants{});
    b_mvo("transactions", fc::variants{});
    b_mvo("hash", to_hex(hash));
    b_mvo("size", "0");
    b_mvo("totalDifficulty", "0");
    return b_mvo;
  }
};

} // namespace noir::eth
