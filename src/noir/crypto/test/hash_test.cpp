// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>

#include <noir/crypto/hash.h>

using namespace noir;
using namespace noir::crypto;

TEST_CASE("hash: keccak256", "[noir][crypto]") {
  auto tests = std::to_array<std::pair<std::string, Bytes>>({
    {"", {"c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"}},
    {"The quick brown fox jumps over the lazy dog",
      {"4d741b6f1eb29cb2a9b9911c82f56fa8d73b04959d3d9d222895df6c0b28aa15"}},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) {
    CHECK(Keccak256()(t.first) == t.second);
  });

  {
    auto hash = Keccak256();
    for (const auto& test : tests) {
      hash.update(test.first);
      CHECK(hash.final() == test.second);
      hash.init();
    }
  }
}

TEST_CASE("hash: sha256", "[noir][crypto]") {
  auto tests = std::to_array<std::pair<std::string, Bytes>>({
    {"", {"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"}},
    {"The quick brown fox jumps over the lazy dog",
      {"d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"}},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) {
    CHECK(Sha256()(t.first) == t.second);
  });

  {
    auto hash = Sha256();
    for (const auto& test : tests) {
      hash.update(test.first);
      CHECK(hash.final() == test.second);
      hash.init();
    }
  }
}

TEST_CASE("hash: blake2b_256", "[noir][crypto]") {
  auto tests = std::to_array<std::pair<std::string, Bytes>>({
    {"", {"0e5751c026e543b2e8ab2eb06099daa1d1e5df47778f7787faab45cdf12fe3a8"}},
    {"The quick brown fox jumps over the lazy dog",
      {"01718cec35cd3d796dd00020e0bfecb473ad23457d063b75eff29c0ffa2e58a9"}},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) {
    CHECK(Blake2b256()(t.first) == t.second);
  });

  {
    auto hash = Blake2b256();
    for (const auto& test : tests) {
      hash.update(test.first);
      CHECK(hash.final() == test.second);
      hash.init();
    }
  }
}

TEST_CASE("hash: ripemd160", "[noir][crypto]") {
  auto tests = std::to_array<std::pair<std::string, Bytes>>({
    {"", {"9c1185a5c5e9fc54612808977ee8f548b2258d31"}},
    {"The quick brown fox jumps over the lazy dog", {"37f332f68db77bd9d7edd4969571ad671cf9dd3b"}},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) {
    CHECK(Ripemd160()(t.first) == t.second);
  });

  {
    auto hash = Ripemd160();
    for (const auto& test : tests) {
      hash.update(test.first);
      CHECK(hash.final() == test.second);
      hash.init();
    }
  }
}

TEST_CASE("hash: sha3_256", "[noir][crypto]") {
  auto tests = std::to_array<std::pair<std::string, Bytes>>({
    {"", {"a7ffc6f8bf1ed76651c14756a061d662f580ff4de43b49fa82d80a4b80f8434a"}},
    {"The quick brown fox jumps over the lazy dog",
      {"69070dda01975c8c120c3aada1b282394e7f032fa9cf32f4cb2259a0897dfc04"}},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) {
    CHECK(Sha3_256()(t.first) == t.second);
  });

  {
    auto hash = Sha3_256();
    for (const auto& test : tests) {
      hash.update(test.first);
      CHECK(hash.final() == test.second);
      hash.init();
    }
  }
}

TEST_CASE("hash: xxhash", "[noir][crypto]") {
  auto tests = std::to_array<std::pair<std::string, uint64_t>>({
    {"", 0xef46db3751d8e999},
    {"The quick brown fox jumps over the lazy dog", 0x0b242d361fda71bc},
  });

  std::for_each(tests.begin(), tests.end(), [&](auto& t) {
    CHECK(Xxh64()(t.first) == t.second);
  });

  {
    auto hash = Xxh64();
    for (const auto& test : tests) {
      hash.update(test.first);
      CHECK(hash.final() == test.second);
      hash.init();
    }
  }
}
