// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/hex.h>
#include <noir/consensus/bit_array.h>
#include <noir/core/types.h>

using namespace noir;
using namespace noir::consensus;

TEST_CASE("Encode and decode bit_array", "[bit_array]") {
  auto bit_array_ = bit_array::new_bit_array(10);
  auto data = core::codec::encode(*bit_array_);
  // std::cout << "data=" << to_hex(data) << std::endl;
  // std::cout << "org = " << bit_array_->string() << std::endl;
  auto decoded = core::codec::decode<bit_array>(data);
  // std::cout << "dec = " << decoded.string() << std::endl;
  CHECK(bit_array_->string() == decoded.string());

  bit_array_->set_index(0, true);
  bit_array_->set_index(2, true);
  data = core::codec::encode(*bit_array_);
  auto decoded2 = core::codec::decode<bit_array>(data);
  CHECK(bit_array_->string() == decoded2.string());

  bit_array_->set_index(9, true);
  data = core::codec::encode(*bit_array_);
  auto decoded3 = core::codec::decode<bit_array>(data);
  CHECK(bit_array_->string() == decoded3.string());

  int TEST_SIZE = 1000;
  auto sa = bit_array::new_bit_array(TEST_SIZE);
  for (auto i = 0; i < TEST_SIZE; i++) {
    sa->set_index(i, i % 3 != 0);
    auto enc = core::codec::encode(*sa);
    auto dec = core::codec::decode<bit_array>(enc);
    CHECK(sa->string() == dec.string());
  }
}

TEST_CASE("Check to_bytes", "[bit_array]") {
  auto ba = bit_array::new_bit_array(4);
  ba->set_index(3, true);
  CHECK(ba->get_bytes() == bytes({'\x10'}));

  ba = bit_array::new_bit_array(9);
  CHECK(ba->get_bytes() == bytes({0, 0}));
  ba->set_index(7, true);
  CHECK(ba->get_bytes() == bytes({1, 0}));
  ba->set_index(8, true);
  CHECK(ba->get_bytes() == bytes({1, '\x80'}));

  ba = bit_array::new_bit_array(16);
  CHECK(ba->get_bytes() == bytes({0, 0}));
  ba->set_index(7, true);
  CHECK(ba->get_bytes() == bytes({1, 0}));
  ba->set_index(8, true);
  CHECK(ba->get_bytes() == bytes({1, '\x80'}));
  ba->set_index(9, true);
  CHECK(ba->get_bytes() == bytes({1, '\xC0'}));
}
