// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash/blake2.h>

namespace noir::crypto {

hash<blake2b_256>& blake2b_256::init() {
  if (!state) {
    state = blake2b_state{};
  }
  blake2b_init(&*state, digest_size());
  return *this;
};

hash<blake2b_256>& blake2b_256::update(std::span<const char> in) {
  if (!state) {
    init();
  }
  blake2b_update(&*state, (const uint8_t*)in.data(), in.size());
  return *this;
}

void blake2b_256::final(std::span<char> out) {
  blake2b_final(&*state, (uint8_t*)out.data(), digest_size());
}

std::size_t blake2b_256::digest_size() const {
  return 32;
}

} // namespace noir::crypto
