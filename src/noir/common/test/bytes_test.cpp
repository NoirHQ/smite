// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>

#include <noir/common/bytes.h>
#include <noir/common/string.h>

using namespace noir;

using Bytes20 = BytesN<20>;
using Bytes32 = BytesN<32>;

TEST_CASE("bytes: variable-length byte sequence", "[noir][common]") {
  SECTION("basic construction") {
    Bytes data({1, 2});
    CHECK(to_string(data) == "0102");
  }

  SECTION("move construction") {
    Bytes from({1, 2});
    void* ptr = from.data();
    Bytes to(std::move(from));
    CHECK(ptr == to.data());
  }
}

TEST_CASE("bytes: fixed-length byte sequence", "[noir][common]") {
  Bytes32 hash{"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};

  SECTION("construction & conversion") {
    std::vector<uint8_t> data = from_hex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    // constructs from hex string
    CHECK(hash == data);

    // constructs from byte sequence
    auto from_span = Bytes32(std::span(hash));
    CHECK(from_span == data);

    // construct from byte vector
    auto from_vec = Bytes32(data);
    CHECK(from_vec == data);

    // implicit copy constructor
    auto copied = Bytes32(hash);
    CHECK(copied == data);

    // implicit move constructor
    auto moved = Bytes32(std::move(Bytes32("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")));
    CHECK(moved == data);
    copied.back() &= 0xf0;
    CHECK(to_string(copied) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b850");
    CHECK(hash == data);

    // implicit copy assignment operator
    auto copy_assigned = hash;
    CHECK(copy_assigned == data);

    // converts to variable-length byte sequence
    auto bytes = Bytes{hash};
    CHECK(bytes.size() == hash.size());
    CHECK(bytes == data);
  }

  SECTION("comparison") {
    Bytes32 empty{};
    CHECK(empty < hash);
    CHECK(empty.empty());

    auto copied = hash;
    CHECK(copied == hash);
    CHECK(!hash.empty());

    // lexicographical comparison between diffrent sized bytesN
    Bytes20 hash20{"ffffffffffffffffffffffffffffffffffffffff"};
    CHECK(hash20 > hash);
    CHECK(!(hash20 == hash));
    CHECK(hash20 != hash);

    hash20 = {"e3b0c44298fc1c149afbf4c8996fb92427ae41e4"};
    CHECK(hash20 < hash);
    CHECK(hash20 != hash);
  }
}
