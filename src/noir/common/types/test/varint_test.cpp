// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/codec/datastream.h>
#include <noir/common/hex.h>
#include <noir/common/types/varint.h>

using namespace noir;
using namespace noir::codec;

TEST_CASE("[common][varint]", "[noir]") {
  std::array<char, 10> buffer;

  SECTION("leb128 - uint32_t") {
    auto tests = std::to_array<std::pair<uint32_t, const char*>>({
      {0, "00"},
      {1, "01"},
      {624485, "e58e26"},
      {0x0fff'ffffu, "ffffff7f"},
      {0x1fff'ffffu, "ffffffff01"},
      {0xffff'ffffu, "ffffffff0f"},
    });

    std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
      basic_datastream<char> ds(buffer);
      auto size = write_uleb128(ds, varint<uint32_t>(t.first));
      CHECK(to_hex({buffer.data(), size}) == t.second);
    });

    std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
      auto buffer = from_hex(t.second);
      basic_datastream<char> ds(buffer);
      varint<uint32_t> v;
      read_uleb128(ds, v);
      CHECK(t.first == v);
    });

    {
      auto buffer = from_hex("8000");
      basic_datastream<char> ds(buffer);
      varint<uint32_t> v;
      CHECK_THROWS_WITH(read_uleb128(ds, v), "uleb128: most significant byte must not be 0");
    }
    {
      auto buffer = from_hex("ffffffff1f");
      basic_datastream<char> ds(buffer);
      varint<uint32_t> v;
      CHECK_THROWS_WITH(read_uleb128(ds, v), "uleb128: unused bits must be cleared");
    }
    {
      auto buffer = from_hex("ffffffff8f");
      basic_datastream<char> ds(buffer);
      varint<uint32_t> v;
      CHECK_THROWS_WITH(read_uleb128(ds, v), "uleb128: unused bits must be cleared");
    }
  }

  SECTION("leb128 - uint64_t") {
    auto tests = std::to_array<std::pair<uint64_t, const char*>>({
      {0, "00"},
      {1, "01"},
      {0x7fff'ffff'ffff'ffffull, "ffffffffffffffff7f"},
      {0xffff'ffff'ffff'ffffull, "ffffffffffffffffff01"},
    });

    std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
      basic_datastream<char> ds(buffer);
      auto size = write_uleb128(ds, varint<uint64_t>(t.first));
      CHECK(to_hex({buffer.data(), size}) == t.second);
    });

    std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
      auto buffer = from_hex(t.second);
      basic_datastream<char> ds(buffer);
      varint<uint64_t> v;
      read_uleb128(ds, v);
      CHECK(t.first == v);
    });

    {
      auto buffer = from_hex("ffffffffffffffffff11");
      basic_datastream<char> ds(buffer);
      varint<uint64_t> v;
      CHECK_THROWS_WITH(read_uleb128(ds, v), "uleb128: unused bits must be cleared");
    }
  }

  SECTION("leb128 - int32_t") {
    auto tests = std::to_array<std::pair<int32_t, const char*>>({
      {0, "00"},
      {1, "01"},
      {2'147'483'647, "ffffffff07"},
      {-1, "ffffffff0f"},
      {-2'147'483'648, "8080808008"},
    });

    std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
      basic_datastream<char> ds(buffer);
      auto size = write_uleb128(ds, varint<int32_t>(t.first));
      CHECK(to_hex({buffer.data(), size}) == t.second);
    });
  }

  SECTION("zigzag - int32_t") {
    auto tests = std::to_array<std::pair<int32_t, const char*>>({
      {0, "00"},
      {-1, "01"},
      {1, "02"},
      {2'147'483'647, "feffffff0f"},
      {-2'147'483'648, "ffffffff0f"},
    });

    std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
      basic_datastream<char> ds(buffer);
      auto size = write_zigzag(ds, varint<int32_t>(t.first));
      CHECK(to_hex({buffer.data(), size}) == t.second);
    });
  }
}
