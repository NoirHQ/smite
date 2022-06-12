// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/crypto/rand.h>

using namespace noir;
using namespace noir::crypto;

TEST_CASE("rand", "[noir][crypto]") {
  std::vector<char> out_vec(8);
  std::array<char, 8> out_arr{};

  CHECK(rand_bytes(out_vec));
  CHECK(rand_bytes(out_arr));
}
