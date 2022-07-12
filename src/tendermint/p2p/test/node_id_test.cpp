// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <tendermint/types/node_id.h>
#include <iostream>

using namespace tendermint;

TEST_CASE("NodeId: node_id_from_pubkey", "[tendermint][p2p]") {
  auto hex_key = "c9a4f86bab6a4383c66fb517ac5eb302c56164c955d490bfe3b0309cb6e98c1e";
  auto pub_key = hex::decode(hex_key, 64);
  auto node_id = node_id_from_pubkey(pub_key);
  CHECK(node_id == "7525a30cd8e51acf87a12b16efd7f4a0af6bd3f5");
}