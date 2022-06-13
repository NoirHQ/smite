// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/codec/bcs.h>

using namespace noir;

TEST_CASE("bcs: Bytes", "[noir][codec]") {
  SECTION("dynamic length") {
    auto bytes = Bytes("01");
    auto encoded = codec::bcs::encode(bytes);
    auto decoded = codec::bcs::decode<Bytes>(encoded);
    CHECK(bytes == decoded);
  }

  SECTION("fixed length") {
    auto bytes = BytesN<1>("01");
    auto encoded = codec::bcs::encode(bytes);
    auto decoded = codec::bcs::decode<BytesN<1>>(encoded);
    CHECK(bytes == decoded);
  }
}
