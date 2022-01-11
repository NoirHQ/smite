// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/hex.h>
#include <noir/consensus/types.h>

namespace noir::consensus {

struct node_key {
  std::string node_id;
  bytes priv_key;

  bytes get_pub_key() {
    return std::vector<char>(from_hex("000000"));
  }

  void save_as() {
    // todo - persist to a file
  }

  static node_key load_or_gen_node_key(std::string file_path) {
    // todo - read from a file
    return gen_node_key();
  }

  static node_key gen_node_key() {
    // todo - generate ed25519 private key
    return node_key{"node_id_abcdefg", std::vector<char>(from_hex("000000"))};
  }

};

} // namespace noir::consensus
