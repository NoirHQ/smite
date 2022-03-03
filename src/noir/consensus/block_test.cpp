// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/consensus/block.h>
#include <noir/core/types.h>
//#include <noir/consensus/common_test.h>

using namespace noir;
using namespace noir::consensus;

TEST_CASE("Encode block", "[block]") {
  block org{block_header{}, block_data{.hash = {0, 1, 2}}, commit{}};
  auto data = core::codec::encode(org);
  auto decoded = core::codec::decode<block>(data);
  CHECK(org.data.hash == decoded.data.hash);
}

TEST_CASE("Make part_set", "[block]") {
  block org{block_header{}, block_data{.hash = {0, 1, 2, 3, 4, 5}}, commit{}};
  uint32_t part_size{block_part_size_bytes};
  // uint32_t part_size{3};
  auto ps = org.make_part_set(part_size);
  auto restored = block::new_block_from_part_set(ps);
  CHECK(org.data.hash == restored->data.hash);
}
