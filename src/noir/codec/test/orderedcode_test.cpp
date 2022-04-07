// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/codec/orderedcode.h>
#include <noir/common/hex.h>
#include <noir/common/str_const.h>
#include <map>

using namespace noir::codec::orderedcode;

TEST_CASE("orderedcode: append and parse uint64", "[noir][codec]") {
  std::map<uint64_t, std::string> testcase;
  testcase.emplace(std::make_pair<uint64_t, std::string>(uint64_t(0), "00"));
  testcase.emplace(std::make_pair<uint64_t, std::string>(uint64_t(1), "0101"));
  testcase.emplace(std::make_pair<uint64_t, std::string>(uint64_t(255), "01ff"));
  testcase.emplace(std::make_pair<uint64_t, std::string>(uint64_t(256), "020100"));
  testcase.emplace(std::make_pair<uint64_t, std::string>(uint64_t(1025), "020401"));
  testcase.emplace(std::make_pair<uint64_t, std::string>(uint64_t(0x0a0b0c0d), "040a0b0c0d"));
  testcase.emplace(std::make_pair<uint64_t, std::string>(uint64_t(0x0102030405060708), "080102030405060708"));
  testcase.emplace(std::make_pair<uint64_t, std::string>(std::numeric_limits<uint64_t>::max(), "08ffffffffffffffff"));

  for (auto& t : testcase) {
    CHECK(noir::to_hex(encode(t.first)) == t.second);
  }

  std::vector<char> v;
  for (auto& t : testcase) {
    v = noir::from_hex(t.second);
    std::uint64_t i;
    decode(v, i);
    CHECK(i == t.first);
  }

  std::vector<char> v2;
  decr<uint64_t> di;
  for (auto& t : testcase) {
    v = encode(decr<std::uint64_t>{t.first});
    std::span<char> sp(v2);
    decode(sp, di);
    CHECK(decr<uint64_t>{t.first} == di);
  }

  auto i1 = uint64_t(0);
  auto i2 = uint64_t(1025);
  auto i3 = uint64_t(0x0a0b0c0d);
  auto i4 = uint64_t(0x0102030405060708);
  auto i5 = std::numeric_limits<uint64_t>::max();

  CHECK(noir::to_hex(encode(i1, i2, i3, i4, i5)) == "00020401040a0b0c0d08010203040506070808ffffffffffffffff");
}

TEST_CASE("orderedcode: append and parse string", "[noir][codec]") {
  std::map<std::string, std::string> testcase;
  testcase.emplace(std::make_pair<std::string, std::string>(str_const(""), "0001"));
  testcase.emplace(std::make_pair<std::string, std::string>(str_const("\x00"), "00ff0001"));
  testcase.emplace(std::make_pair<std::string, std::string>(str_const("\x00\x00"), "00ff00ff0001"));
  testcase.emplace(std::make_pair<std::string, std::string>(str_const("\x01"), "010001"));
  testcase.emplace(std::make_pair<std::string, std::string>(str_const("foo\x00"), "666f6f00ff0001"));
  testcase.emplace(std::make_pair<std::string, std::string>(str_const("foo\x00\x01"), "666f6f00ff010001"));
  testcase.emplace(std::make_pair<std::string, std::string>(str_const("foo\x01"), "666f6f010001"));
  testcase.emplace(std::make_pair<std::string, std::string>(str_const("foo\x01\x00"), "666f6f0100ff0001"));
  testcase.emplace(std::make_pair<std::string, std::string>(str_const("foo\xfe"), "666f6ffe0001"));
  testcase.emplace(std::make_pair<std::string, std::string>(str_const("foo\xff"), "666f6fff000001"));
  testcase.emplace(std::make_pair<std::string, std::string>(str_const("\xff"), "ff000001"));
  testcase.emplace(std::make_pair<std::string, std::string>(str_const("\xff\xff"), "ff00ff000001"));

  for (auto& t : testcase) {
    CHECK(noir::to_hex(encode(t.first)) == t.second);
  }

  std::vector<char> v;
  for (auto& t : testcase) {
    v = noir::from_hex(t.second);
    std::string s;
    decode(v, s);
    CHECK(s == t.first);
  }

  std::vector<char> v2;
  decr<std::string> ds;
  for (auto& t : testcase) {
    v2 = encode(decr<std::string>{t.first});
    std::span<char> sp(v2);
    decode(sp, ds);
    CHECK(decr<std::string>{t.first} == ds);
  }

  std::string s1 = str_const("");
  std::string s2 = str_const("\x00");
  std::string s3 = str_const("foo\x00\x01");
  std::string s4 = str_const("foo\xfe");
  std::string s5 = str_const("\xff\xff");

  CHECK(noir::to_hex(encode(s1, s2, s3, s4, s5)) == "000100ff0001666f6f00ff010001666f6ffe0001ff00ff000001");
}

TEST_CASE("orderedcode: append and parse int64_t", "[noir][codec]") {
  std::map<int64_t, std::string> testcase;

  testcase.emplace(std::make_pair<int64_t, std::string>(-8193, "1fdfff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(-8192, "2000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(-4097, "2fff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(-257, "3eff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(-256, "3f00"));
  testcase.emplace(std::make_pair<int64_t, std::string>(-66, "3fbe"));
  testcase.emplace(std::make_pair<int64_t, std::string>(-65, "3fbf"));
  testcase.emplace(std::make_pair<int64_t, std::string>(-64, "40"));
  testcase.emplace(std::make_pair<int64_t, std::string>(-63, "41"));
  testcase.emplace(std::make_pair<int64_t, std::string>(-3, "7d"));
  testcase.emplace(std::make_pair<int64_t, std::string>(-2, "7e"));
  testcase.emplace(std::make_pair<int64_t, std::string>(-1, "7f"));
  testcase.emplace(std::make_pair<int64_t, std::string>(0, "80"));
  testcase.emplace(std::make_pair<int64_t, std::string>(1, "81"));
  testcase.emplace(std::make_pair<int64_t, std::string>(2, "82"));
  testcase.emplace(std::make_pair<int64_t, std::string>(62, "be"));
  testcase.emplace(std::make_pair<int64_t, std::string>(63, "bf"));
  testcase.emplace(std::make_pair<int64_t, std::string>(64, "c040"));
  testcase.emplace(std::make_pair<int64_t, std::string>(65, "c041"));
  testcase.emplace(std::make_pair<int64_t, std::string>(255, "c0ff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(256, "c100"));
  testcase.emplace(std::make_pair<int64_t, std::string>(4096, "d000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(8191, "dfff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(8192, "e02000"));

  testcase.emplace(std::make_pair<int64_t, std::string>(-0x800, "3800"));
  testcase.emplace(std::make_pair<int64_t, std::string>(0x424242, "f0424242"));
  testcase.emplace(std::make_pair<int64_t, std::string>(0x23, "a3"));
  testcase.emplace(std::make_pair<int64_t, std::string>(0x10e, "c10e"));
  testcase.emplace(std::make_pair<int64_t, std::string>(-0x10f, "3ef1"));
  testcase.emplace(std::make_pair<int64_t, std::string>(0x020b0c0d, "f20b0c0d"));
  testcase.emplace(std::make_pair<int64_t, std::string>(0x0a0b0c0d, "f80a0b0c0d"));
  testcase.emplace(std::make_pair<int64_t, std::string>(0x0102030405060708, "ff8102030405060708"));

  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 63) - 0, "003f8000000000000000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 62) - 1, "003fbfffffffffffffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 62) - 0, "004000000000000000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 55) - 1, "007f7fffffffffffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 55) - 0, "0080000000000000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 48) - 1, "00feffffffffffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 48) - 0, "01000000000000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 41) - 1, "01fdffffffffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 41) - 0, "020000000000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 34) - 1, "03fbffffffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 34) - 0, "0400000000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 27) - 1, "07f7ffffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 27) - 0, "08000000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 20) - 1, "0fefffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 20) - 0, "100000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 13) - 1, "1fdfff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 13) - 0, "2000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 6) - 1, "3fbf"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)-1 << 6) - 0, "40"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 6) - 1, "bf"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 6) - 0, "c040"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 13) - 1, "dfff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 13) - 0, "e02000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 20) - 1, "efffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 20) - 0, "f0100000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 27) - 1, "f7ffffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 27) - 0, "f808000000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 34) - 1, "fbffffffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 34) - 0, "fc0400000000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 41) - 1, "fdffffffffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 41) - 0, "fe020000000000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 48) - 1, "feffffffffffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 48) - 0, "ff01000000000000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 55) - 1, "ff7fffffffffffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 55) - 0, "ff8080000000000000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 62) - 1, "ffbfffffffffffffff"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 62) - 0, "ffc04000000000000000"));
  testcase.emplace(std::make_pair<int64_t, std::string>(((int64_t)1 << 63) - 1, "ffc07fffffffffffffff"));

  for (auto& t : testcase) {
    CHECK(noir::to_hex(encode(t.first)) == t.second);
  }

  std::vector<char> v;
  for (auto& t : testcase) {
    v = noir::from_hex(t.second);
    std::int64_t i;
    decode(v, i);
    CHECK(i == t.first);
  }

  std::vector<char> v2;
  decr<int64_t> di;
  for (auto& t : testcase) {
    v2 = encode(decr<int64_t>{t.first});
    std::span<char> sp(v2);
    decode(sp, di);
    CHECK(decr<int64_t>{t.first} == di);
  }

  int64_t i1 = 0;
  int64_t i2 = -0x800;
  int64_t i3 = 0x424242;
  int64_t i4 = ((int64_t)-1 << 63) - 0;
  int64_t i5 = ((int64_t)1 << 62) - 0;

  CHECK(noir::to_hex(encode(i1, i2, i3, i4, i5)) == "803800f0424242003f8000000000000000ffc04000000000000000");
}

TEST_CASE("orderedcode: append double", "[noir][codec]") {
  std::map<double, std::string> testcase;

  testcase.emplace(
    std::make_pair<double, std::string>(-std::numeric_limits<double>::infinity(), "003f8010000000000000"));
  testcase.emplace(std::make_pair<double, std::string>(-std::numeric_limits<double>::max(), "003f8010000000000001"));
  testcase.emplace(std::make_pair<double, std::string>(-2.71828, "003fbffa40f66a550870"));
  testcase.emplace(std::make_pair<double, std::string>(-1.0, "004010000000000000"));
  testcase.emplace(std::make_pair<double, std::string>(0.0, "80"));
  testcase.emplace(std::make_pair<double, std::string>(0.333333333, "ffbfd5555554f9b516"));
  testcase.emplace(std::make_pair<double, std::string>(1.0, "ffbff0000000000000"));
  testcase.emplace(std::make_pair<double, std::string>(1.41421, "ffbff6a09aaa3ad18d"));
  testcase.emplace(std::make_pair<double, std::string>(1.5, "ffbff8000000000000"));
  testcase.emplace(std::make_pair<double, std::string>(2.0, "ffc04000000000000000"));
  testcase.emplace(std::make_pair<double, std::string>(3.14159, "ffc0400921f9f01b866e"));
  testcase.emplace(std::make_pair<double, std::string>(6.022e23, "ffc044dfe154f457ea13"));
  testcase.emplace(std::make_pair<double, std::string>(std::numeric_limits<double>::max(), "ffc07fefffffffffffff"));
  testcase.emplace(
    std::make_pair<double, std::string>(std::numeric_limits<double>::infinity(), "ffc07ff0000000000000"));

  for (auto& t : testcase) {
    CHECK(noir::to_hex(encode(t.first)) == t.second);
  }

  std::vector<char> v;
  for (auto& t : testcase) {
    v = noir::from_hex(t.second);
    double d;
    decode(v, d);
    CHECK(d == t.first);
  }

  std::vector<char> v2;
  decr<double> dd;
  for (auto& t : testcase) {
    v2 = encode(decr<double>{t.first});
    std::span<char> sp(v2);
    decode(sp, dd);
    CHECK(decr<double>{t.first} == dd);
  }

  double d1 = -std::numeric_limits<double>::max();
  double d2 = -2.71828;
  double d3 = 0.333333333;
  double d4 = 6.022e23;
  double d5 = std::numeric_limits<double>::max();

  CHECK(noir::to_hex(encode(d1, d2, d3, d4, d5)) ==
    "003f8010000000000001003fbffa40f66a550870ffbfd5555554f9b516ffc044dfe154f457ea13ffc07fefffffffffffff");
}

TEST_CASE("orderedcode: append infinity", "[noir][codec]") {
  CHECK(noir::to_hex(encode(infinity{})) == "ffff");
}

TEST_CASE("orderedcode: increase decrease", "[noir][codec]") {
  CHECK(noir::to_hex(encode(uint64_t(0), decr<uint64_t>{1}, uint64_t(2), decr<uint64_t>{516}, uint64_t(517),
          decr<uint64_t>{0})) == "00fefe0102fdfdfb020205ff");
}

TEST_CASE("orderedcode: parse infinity", "[noir][codec]") {
  std::vector<const char> v = {char(0xff), char(0xff)};
  std::span<const char> sp(v);

  infinity i;
  CHECK_NOTHROW(decode(sp, i));
}

TEST_CASE("orderedcode: append trailing string", "[noir][codec]") {
  std::vector<std::string> testcase = {str_const(""), str_const("\x00"), str_const("\x00\x01"), str_const("a"),
    str_const("bcd"), str_const("foo\x00"), str_const("foo\x00bar"), str_const("foo\x00bar\x00"), str_const("\xff"),
    str_const("\xff\x00"), str_const("\xff\xfe"), str_const("\xff\xff")};

  std::vector<char> b;
  for (auto& t : testcase) {
    b.insert(b.end(), t.begin(), t.end());
    CHECK(noir::to_hex(encode(trailing_string{t})) == noir::to_hex(b));
    b.clear();
  }
}

TEST_CASE("orderedcode: append string or infinity", "[noir][codec]") {
  string_or_infinity soi1{.s = "foo"};
  string_or_infinity soi2{.inf = true};
  string_or_infinity soi3{.s = "foo", .inf = true};

  CHECK(noir::to_hex(encode(soi1)) == "666f6f0001");
  CHECK(noir::to_hex(encode(soi2)) == "ffff");
  CHECK_THROWS(encode(soi3));

  decr<string_or_infinity> dsoi1{.val.s = str_const("\00")};
  decr<string_or_infinity> dsoi2{.val.inf = true};
  decr<string_or_infinity> dsoi3{.val.s = str_const("\00"), .val.inf = true};

  CHECK(noir::to_hex(encode(dsoi1)) == "ff00fffe");
  CHECK(noir::to_hex(encode(dsoi2)) == "0000");
  CHECK_THROWS(encode(dsoi3));
}
