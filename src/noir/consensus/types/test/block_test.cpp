// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/types/block.h>
#include <noir/core/codec.h>

using namespace noir;
using namespace noir::consensus;

TEST_CASE("block: encode using datastream", "[noir][consensus]") {
  block org{block_header{}, block_data{.txs = {{0}, {1}, {2}}}, {}, std::make_unique<commit>()};
  auto data = encode(org);
  auto decoded = decode<block>(data);
  CHECK(org.data.get_hash() == decoded.data.get_hash());
}

TEST_CASE("block: encode using protobuf", "[noir][consensus]") {
  block org{block_header{}, block_data{.txs = {{0}, {1}, {2}}}, {}, nullptr};
  auto pb = block::to_proto(org);
  Bytes bz(pb->ByteSizeLong());
  pb->SerializeToArray(bz.data(), pb->ByteSizeLong());

  ::tendermint::types::Block pb_block;
  pb_block.ParseFromArray(bz.data(), bz.size());
  auto decoded = block::from_proto(pb_block);
  CHECK(org.data.get_hash() == decoded->data.get_hash());
}

TEST_CASE("block: Make part_set", "[noir][consensus]") {
  block org{block_header{}, block_data{.hash = {0, 1, 2, 3, 4, 5}}, {}, nullptr};
  uint32_t part_size{block_part_size_bytes};
  // uint32_t part_size{3};
  auto ps = org.make_part_set(part_size);
  auto restored = block::new_block_from_part_set(ps);
  CHECK(org.data.hash == restored->data.hash);
}
