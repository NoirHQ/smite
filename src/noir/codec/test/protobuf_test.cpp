// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/codec/protobuf.h>
#include <noir/codec/protobuf/refl.h>
#include <noir/common/hex.h>

using namespace noir;
using namespace noir::codec::protobuf;

template<typename T>
void check_conversion(const T& v, std::string_view s) {
  auto data = encode(v);
  CHECK(to_hex(data) == s);
  CHECK(v == decode<T>(data));
}

TEST_CASE("protobuf: primitive types", "[noir][codec]") {
  check_conversion<int32_t>(300, "ac02");
  check_conversion<sint32>(0, "00");
  check_conversion<sint32>(-1, "01");
  check_conversion<sint32>(1, "02");
  check_conversion<sint32>(-2, "03");
  check_conversion<sint32>(std::numeric_limits<int32_t>::max(), to_hex(encode(uint32_t(-1) - 1)));
  check_conversion<sint32>(std::numeric_limits<int32_t>::min(), to_hex(encode(uint32_t(-1))));
}

struct Test1 {
  std::optional<int32_t> a; // = 1
};
PROTOBUF_REFLECT(Test1, (a, 1));

struct Test2 {
  std::optional<std::string> b; // = 2
};
PROTOBUF_REFLECT(Test2, (b, 2));

struct Test3 {
  std::optional<Test1> c; // = 3
};
PROTOBUF_REFLECT(Test3, (c, 3));

struct Test4 {
  std::optional<std::vector<int32_t>> d; // = 4
};
PROTOBUF_REFLECT(Test4, (d, 4));

TEST_CASE("protobuf: messages", "[noir][codec]") {
  {
    Test1 v{150};
    auto data = encode(v);
    CHECK(to_hex(data) == "089601");
    CHECK(v.a == decode<Test1>(data).a);
  }

  {
    Test2 v{"testing"};
    auto data = encode(v);
    CHECK(to_hex(data) == "120774657374696e67");
    CHECK(v.b == decode<Test2>(data).b);
  }

  {
    Test3 v{Test1{150}};
    auto data = encode(v);
    CHECK(to_hex(data) == "1a03089601");
    CHECK(v.c->a == decode<Test3>(data).c->a);
  }

  {
    Test4 v{std::vector<int32_t>{3, 270, 86942}};
    auto data = encode(v);
    CHECK(to_hex(data) == "2206038e029ea705");
    CHECK(v.d == decode<Test4>(data).d);
  }
}