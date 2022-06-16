// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash/xxhash.h>

namespace noir::crypto {

auto Xxh64::init() -> Xxh64& {
  if (!state) {
    state.emplace();
  }
  XXH64_reset(&*state, 0);
  return *this;
}

auto Xxh64::update(std::span<const unsigned char> in) -> Xxh64& {
  if (!state) {
    init();
  }
  XXH64_update(&*state, in.data(), in.size());
  return *this;
}

void Xxh64::final(std::span<unsigned char> out) {
  auto hash = XXH64_digest(&*state);
  std::memcpy(out.data(), (const unsigned char*)&hash, sizeof(decltype(hash)));
}

auto Xxh64::final() -> uint64_t {
  return XXH64_digest(&*state);
}

} // namespace noir::crypto
