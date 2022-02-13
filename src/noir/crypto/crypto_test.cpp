// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/hex.h>
#include <noir/crypto/hash.h>

using namespace noir;
using namespace noir::crypto;

TEST_CASE("[hash] keccak256", "[crypto]") {
  auto tests = std::to_array<std::pair<std::string, std::string>>({
    {"", "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"},
    {"The quick brown fox jumps over the lazy dog", "4d741b6f1eb29cb2a9b9911c82f56fa8d73b04959d3d9d222895df6c0b28aa15"},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) { CHECK(to_hex(keccak256(t.first)) == t.second); });
}

TEST_CASE("[hash] sha256", "[crypto]") {
  auto tests = std::to_array<std::pair<std::string, std::string>>({
    {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
    {"The quick brown fox jumps over the lazy dog", "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) { CHECK(to_hex(sha256(t.first)) == t.second); });
}

TEST_CASE("[hash] blake2b_256", "[crypto]") {
  auto tests = std::to_array<std::pair<std::string, std::string>>({
    {"", "0e5751c026e543b2e8ab2eb06099daa1d1e5df47778f7787faab45cdf12fe3a8"},
    {"The quick brown fox jumps over the lazy dog", "01718cec35cd3d796dd00020e0bfecb473ad23457d063b75eff29c0ffa2e58a9"},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) { CHECK(to_hex(blake2b_256(t.first)) == t.second); });
}

TEST_CASE("[hash] ripemd160", "[crypto]") {
  auto tests = std::to_array<std::pair<std::string, std::string>>({
    {"", "9c1185a5c5e9fc54612808977ee8f548b2258d31"},
    {"The quick brown fox jumps over the lazy dog", "37f332f68db77bd9d7edd4969571ad671cf9dd3b"},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) { CHECK(to_hex(ripemd160(t.first)) == t.second); });
}