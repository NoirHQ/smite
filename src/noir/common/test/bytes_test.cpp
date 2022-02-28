// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/codec/scale.h>
#include <noir/common/types.h>

using namespace noir;

TEST_CASE("[common][bytes] variable-length byte sequence", "[noir]") {
  bytes data{1, 2};
  CHECK(to_string(data) == "0102");
}

TEST_CASE("[common][bytes] fixed-length byte sequence", "[noir]") {
  bytes32 hash{"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};

  SECTION("construction & conversion") {
    // constructs from hex string
    CHECK(hash.to_string() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // constructs from byte sequence
    auto from_span = bytes32(hash.to_span());
    CHECK(from_span.to_string() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // construct from byte vector
    std::vector<uint8_t> data = {0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f,
      0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55};
    auto from_vec = bytes32(data);
    CHECK(from_vec.to_string() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // implicit copy constructor
    auto copied = bytes32(hash);
    CHECK(copied.to_string() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // implicit move constructor
    auto moved = bytes32(std::move(bytes32("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
    CHECK(moved.to_string() == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    copied.back() &= 0xf0;
    CHECK(to_string(copied) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b850");
    CHECK(to_string(hash) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // implicit copy assignment operator
    auto copy_assigned = hash;
    CHECK(to_string(copy_assigned) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // converts to variable-length byte sequence
    auto bytes = hash.to_bytes();
    CHECK((bytes.size() == hash.size() && to_hex({bytes.data(), bytes.size()}) == to_string(hash)));
  }

  SECTION("serialize") {
    bytes32 empty{};
    CHECK(to_string(empty) == "0000000000000000000000000000000000000000000000000000000000000000");

    codec::scale::datastream<char> ds(empty.to_span());
    ds << hash;
    CHECK(to_string(empty) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // clear() fills bytesN with 0
    empty.clear();
    CHECK(to_string(empty) == "0000000000000000000000000000000000000000000000000000000000000000");

    ds = {hash.to_span()};
    ds >> empty;
    CHECK(to_string(empty) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  }

  SECTION("base type") {
    using sbytes32 = bytes_n<32, char>;
    using ubytes32 = bytes_n<32, unsigned char>;

    sbytes32 shash{"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};

    auto uhash = ubytes32{shash.to_span()};
    CHECK(to_string(uhash) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  }

  SECTION("comparison") {
    bytes32 empty{};
    CHECK(empty < hash);
    CHECK(empty.empty());

    auto copied = hash;
    CHECK(copied == hash);
    CHECK(!hash.empty());

    // lexicographical comparison between diffrent sized bytesN
    bytes20 hash20{"ffffffffffffffffffffffffffffffffffffffff"};
    CHECK(hash20 > hash);
    CHECK(!(hash20 == hash));
    CHECK(hash20 != hash);

    hash20 = {"e3b0c44298fc1c149afbf4c8996fb92427ae41e4"};
    CHECK(hash20 < hash);
    CHECK(hash20 != hash);
  }
}
