// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/codec/scale.h>
#include <noir/common/hex.h>
#include <noir/consensus/bit_array.h>

using namespace noir;
using namespace noir::consensus;

TEST_CASE("Encode bit_array", "[bit_array]") {
  auto bit_array_ = bit_array::new_bit_array(9);
  auto data = codec::scale::encode(*bit_array_);
  // std::cout << "data=" << to_hex(data) << std::endl;
  // std::cout << "org = " << bit_array_->string() << std::endl;
  auto decoded = codec::scale::decode<bit_array>(data);
  // std::cout << "dec = " << decoded.string() << std::endl;
  CHECK(bit_array_->string() == decoded.string());

  bit_array_->set_index(0, true);
  bit_array_->set_index(2, true);
  data = codec::scale::encode(*bit_array_);
  auto decoded2 = codec::scale::decode<bit_array>(data);
  CHECK(bit_array_->string() == decoded2.string());

  bit_array_->set_index(5, true);
  data = codec::scale::encode(*bit_array_);
  auto decoded3 = codec::scale::decode<bit_array>(data);
  CHECK(bit_array_->string() == decoded3.string());
}
