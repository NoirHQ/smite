// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/hex.h>
#include <fmt/core.h>

using namespace noir;

template<typename TestType, size_t N>
void test_hex_conversion(const std::array<std::pair<TestType, const char*>, N>& tests) {
  std::for_each(tests.begin(), tests.end(), [](const auto& t) {
    TestType v = 0;
    from_hex(t.second, v);
    CHECK(t.first == v);
    CHECK((to_hex(t.first) == t.second || fmt::format("0x{}", to_hex(t.first)) == t.second));
  });
}

TEST_CASE("hex", "[noir][common]") {
  SECTION("uint8_t") {
    auto tests = std::to_array<std::pair<uint8_t, const char*>>({
      {0, "00"},
      {1, "01"},
      {42, "2a"},
      {128, "80"},
      {255, "ff"},
      {0, "0x00"},
      {1, "0x01"},
      {42, "0x2a"},
      {128, "0x80"},
      {255, "0xff"},
    });

    test_hex_conversion(tests);
  }

  SECTION("uint16_t") {
    auto tests = std::to_array<std::pair<uint16_t, const char*>>({
      {0, "0000"},
      {1, "0001"},
      {32767, "7fff"},
      {32768, "8000"},
      {65535, "ffff"},
      {0, "0x0000"},
      {1, "0x0001"},
      {32767, "0x7fff"},
      {32768, "0x8000"},
      {65535, "0xffff"},
    });

    test_hex_conversion(tests);

    {
      auto tests = std::to_array<std::pair<uint16_t, const char*>>({
        {0, "0x"},
        {0, "0x0"},
        {1, "0x1"},
        {61439, "0x1efff"},
      });

      std::for_each(tests.begin(), tests.end(), [&](const auto& t) {
        uint16_t v = 0;
        CHECK_NOTHROW(from_hex(t.second, v));
        CHECK(v == t.first);
      });
    }
  }

  SECTION("uint32_t") {
    auto tests = std::to_array<std::pair<uint32_t, const char*>>({
      {0, "00000000"},
      {1, "00000001"},
      {2'147'483'647, "7fffffff"},
      {2'147'483'648, "80000000"},
      {4'294'967'295, "ffffffff"},
      {0, "0x00000000"},
      {1, "0x00000001"},
      {2'147'483'647, "0x7fffffff"},
      {2'147'483'648, "0x80000000"},
      {4'294'967'295, "0xffffffff"},
    });

    test_hex_conversion(tests);
  }

  SECTION("uint64_t") {
    auto tests = std::to_array<std::pair<uint64_t, const char*>>({
      {0, "0000000000000000"},
      {1, "0000000000000001"},
      {9'223'372'036'854'775'807ull, "7fffffffffffffff"},
      {9'223'372'036'854'775'808ull, "8000000000000000"},
      {18'446'744'073'709'551'615ull, "ffffffffffffffff"},
      {0, "0x0000000000000000"},
      {1, "0x0000000000000001"},
      {9'223'372'036'854'775'807ull, "0x7fffffffffffffff"},
      {9'223'372'036'854'775'808ull, "0x8000000000000000"},
      {18'446'744'073'709'551'615ull, "0xffffffffffffffff"},
    });

    test_hex_conversion(tests);
  }

  SECTION("uint128_t") {
    uint128_t med = (uint128_t)uint64_t(-1) + 1;
    uint128_t max = (uint128_t)(uint64_t(-1)) << 64 | uint64_t(-1);

    auto tests = std::to_array<std::pair<uint128_t, const char*>>({
      {0, "00000000000000000000000000000000"},
      {1, "00000000000000000000000000000001"},
      {med, "00000000000000010000000000000000"},
      {max, "ffffffffffffffffffffffffffffffff"},
      {0, "0x00000000000000000000000000000000"},
      {1, "0x00000000000000000000000000000001"},
      {med, "0x00000000000000010000000000000000"},
      {max, "0xffffffffffffffffffffffffffffffff"},
    });

    test_hex_conversion(tests);
  }

  SECTION("uint256_t") {
    auto v = uint256_t("0xfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141");
    CHECK(to_hex(v) == "fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141");
  }
}
