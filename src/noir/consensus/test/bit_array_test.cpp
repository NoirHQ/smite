// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/hex.h>
#include <noir/consensus/bit_array.h>
#include <noir/core/codec.h>

using namespace noir;
using namespace noir::consensus;

TEST_CASE("bit_array: Check to_bytes", "[noir][consensus]") {
  auto ba = bit_array::new_bit_array(4);
  ba->set_index(0, true);
  CHECK(ba->get_bytes() == Bytes("01"));
  ba->set_index(3, true);
  CHECK(ba->get_bytes() == Bytes("09"));

  ba = bit_array::new_bit_array(9);
  CHECK(ba->get_bytes() == Bytes("0000"));
  ba->set_index(7, true);
  CHECK(ba->get_bytes() == Bytes("8000"));
  ba->set_index(8, true);
  CHECK(ba->get_bytes() == Bytes("8001"));

  ba = bit_array::new_bit_array(16);
  CHECK(ba->get_bytes() == Bytes("0000"));
  ba->set_index(7, true);
  CHECK(ba->get_bytes() == Bytes("8000"));
  ba->set_index(8, true);
  CHECK(ba->get_bytes() == Bytes("8001"));
  ba->set_index(9, true);
  CHECK(ba->get_bytes() == Bytes("8003"));
}

TEST_CASE("bit_array: Check operations", "[noir][consensus]") {
  SECTION("constructors") {
    auto ba = bit_array::new_bit_array(10);
    auto bb = ba;
    bb->set_index(4, true);
    CHECK(bb->size() == 10);
    CHECK(bb->get_index(4));
    auto bc = std::move(bb);
    CHECK(bc->size() == 10);
    CHECK(bc->get_index(4));
  }

  SECTION("size_words") {
    auto ba = bit_array::new_bit_array(0);
    CHECK(ba->num_elems(ba->size()) == 0);
    ba = bit_array::new_bit_array(10);
    CHECK(ba->num_elems(ba->size()) == 1);
    ba = bit_array::new_bit_array(65);
    CHECK(ba->num_elems(ba->size()) == 2);
  }

  SECTION("set/get") {
    auto ba = bit_array::new_bit_array(65);
    CHECK(ba->set_index(4, true));
    CHECK(ba->set_index(64, true));
    CHECK(ba->set_index(65, true) == false);
    CHECK(ba->get_bytes()[0] == (1 << 4));
    CHECK(ba->get_bytes()[1] == 0);
    CHECK(ba->get_bytes()[8] == 1);
    CHECK((ba->get_index(4) && !ba->get_index(63) && ba->get_index(64)));
  }

  SECTION("update") {
    auto ba = bit_array::new_bit_array(10);
    auto bb = bit_array::new_bit_array(16);
    bb->set_index(4, true);
    bb->set_index(12, true);
    CHECK(ba->get_index(4) == false);
    ba->update(bb);
    CHECK(ba->size() == 10);
    CHECK(ba->get_index(4));
    CHECK(ba->get_index(12) == false);
  }

  SECTION("sub") {
    {
      auto ba = bit_array::new_bit_array(60);
      auto bb = bit_array::new_bit_array(66);
      ba->set_index(0, true);
      ba->set_index(1, true);
      bb->set_index(1, true);
      bb->set_index(2, true);
      bb->set_index(64, true);
      ba = ba->sub(bb);
      CHECK(ba->get_index(0));
      CHECK(ba->get_index(1) == false);
      CHECK(ba->get_index(2) == false);
      CHECK(ba->get_index(3) == false);
    }

    {
      auto ba = bit_array::new_bit_array(66);
      auto bb = bit_array::new_bit_array(60);
      ba->set_index(0, true);
      ba->set_index(1, true);
      ba->set_index(64, true);
      bb->set_index(1, true);
      bb->set_index(2, true);
      ba = ba->sub(bb);
      CHECK(ba->get_index(0));
      CHECK(ba->get_index(1) == false);
      CHECK(ba->get_index(2) == false);
      CHECK(ba->get_index(3) == false);
      CHECK(ba->get_index(64));
      CHECK(ba->get_index(65) == false);
    }
  }

  SECTION("or") {
    {
      auto ba = bit_array::new_bit_array(60);
      auto bb = bit_array::new_bit_array(66);
      ba->set_index(0, true);
      ba->set_index(1, true);
      bb->set_index(1, true);
      bb->set_index(2, true);
      bb->set_index(64, true);
      ba = ba->or_op(bb);
      CHECK(ba->size() == 66);
      CHECK(ba->get_index(0));
      CHECK(ba->get_index(1));
      CHECK(ba->get_index(2));
      CHECK(ba->get_index(3) == false);
    }

    {
      auto ba = bit_array::new_bit_array(66);
      auto bb = bit_array::new_bit_array(60);
      ba->set_index(0, true);
      ba->set_index(1, true);
      ba->set_index(64, true);
      bb->set_index(1, true);
      bb->set_index(2, true);
      ba = ba->or_op(bb);
      CHECK(ba->size() == 66);
      CHECK(ba->get_index(0));
      CHECK(ba->get_index(1));
      CHECK(ba->get_index(2));
      CHECK(ba->get_index(3) == false);
      CHECK(ba->get_index(64));
    }
  }

  SECTION("not") {
    auto ba = bit_array::new_bit_array(10);
    ba->set_index(4, true);
    ba = ba->not_op();
    CHECK(ba->get_index(0));
    CHECK(ba->get_index(4) == false);
  }
}

TEST_CASE("bit_array: serialization", "[noir][consensus]") {
  SECTION("datastream") {
    auto bit_array_ = bit_array::new_bit_array(10);
    auto data = encode(*bit_array_);
    auto decoded = decode<bit_array>(data);
    CHECK(bit_array_->string() == decoded.string());

    bit_array_->set_index(0, true);
    bit_array_->set_index(2, true);
    data = encode(*bit_array_);
    auto decoded2 = decode<bit_array>(data);
    CHECK(bit_array_->string() == decoded2.string());

    bit_array_->set_index(9, true);
    data = encode(*bit_array_);
    auto decoded3 = decode<bit_array>(data);
    CHECK(bit_array_->string() == decoded3.string());

    int TEST_SIZE = 1000;
    auto sa = bit_array::new_bit_array(TEST_SIZE);
    for (auto i = 0; i < TEST_SIZE; i++) {
      sa->set_index(i, i % 3 != 0);
      auto enc = encode(*sa);
      auto dec = decode<bit_array>(enc);
      CHECK(sa->string() == dec.string());
    }
  }

  SECTION("protobuf") {
    for (auto i = 0; i < 10; i++) {
      auto ba = bit_array::new_bit_array(i);
      auto pb = bit_array::to_proto(*ba);
      auto restored = bit_array::from_proto(*pb);
      CHECK(ba->size() == restored->size());
      CHECK(ba->string() == restored->string());
    }
    auto ba = bit_array::new_bit_array(65);
    ba->set_index(4, true);
    ba->set_index(64, true);
    auto pb = bit_array::to_proto(*ba);
    Bytes bz(pb->ByteSizeLong());
    pb->SerializeToArray(bz.data(), pb->ByteSizeLong());
    CHECK(bz == Bytes("084112021001"));
    ::tendermint::libs::bits::BitArray pb_restored;
    pb_restored.ParseFromArray(bz.data(), bz.size());
    auto restored = bit_array::from_proto(pb_restored);
    CHECK(ba->string() == restored->string());
  }
}
