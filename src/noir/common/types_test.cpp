// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/codec/scale.h>
#include <noir/common/types.h>

using namespace noir;

bytes32 hash{"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};

TEST_CASE("[bytes] construction", "[common]") {
  CHECK(to_string(hash) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

  auto copied = hash;
  CHECK(to_string(copied) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

  copied.back() &= 0xf0;
  CHECK(to_string(copied) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b850");
  CHECK(to_string(hash) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

  auto bytes = hash.to_bytes();
  CHECK((bytes.size() == hash.size() && to_hex({bytes.data(), bytes.size()}) == to_string(hash)));
}

TEST_CASE("[bytes] serialize", "[common]") {
  bytes32 empty{};
  CHECK(to_string(empty) == "0000000000000000000000000000000000000000000000000000000000000000");

  codec::scale::datastream<char> ds(empty.to_span());
  ds << hash;
  CHECK(to_string(empty) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

  empty.clear();
  CHECK(to_string(empty) == "0000000000000000000000000000000000000000000000000000000000000000");

  ds = {hash.to_span()};
  ds >> empty;
  CHECK(to_string(empty) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE("[bytes] base type", "[common]") {
  using sbytes32 = bytesN<32, char>;
  using ubytes32 = bytesN<32, unsigned char>;

  sbytes32 shash{"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};

  auto uhash = ubytes32{shash.to_span()};
  CHECK(to_string(uhash) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE("[bytes] comparison", "[common]") {
  bytes32 empty{};
  CHECK(empty < hash);
  CHECK(empty.empty());

  auto copied = hash;
  CHECK(copied == hash);
  CHECK(!hash.empty());

  bytes20 hash20{"ffffffffffffffffffffffffffffffffffffffff"};
  CHECK(hash20 > hash);
  CHECK(!(hash20 == hash));
  CHECK(hash20 != hash);

  hash20 = {"e3b0c44298fc1c149afbf4c8996fb92427ae41e4"};
  CHECK(hash20 < hash);
  CHECK(hash20 != hash);
}
