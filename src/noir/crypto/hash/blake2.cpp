// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash/blake2.h>

namespace noir::crypto {

auto Blake2b256::init() -> Blake2b256& {
  if (!state) {
    state.emplace();
  }
  blake2b_init(&*state, digest_size());
  return *this;
}

auto Blake2b256::update(std::span<const unsigned char> in) -> Blake2b256& {
  if (!state) {
    init();
  }
  blake2b_update(&*state, in.data(), in.size());
  return *this;
}

void Blake2b256::final(std::span<unsigned char> out) {
  blake2b_final(&*state, out.data(), digest_size());
}

} // namespace noir::crypto
