// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/codec/scale.h>
#include <noir/consensus/block.h>
//#include <noir/consensus/common_test.h>

using namespace noir;
using namespace noir::consensus;

TEST_CASE("Encode block", "[block]") {
  block org{};
  auto data = codec::scale::encode(org);
  auto decoded = codec::scale::decode<block>(data);
  CHECK(org.data.hash == decoded.data.hash);
}

TEST_CASE("Make part_set", "[block]") {}
