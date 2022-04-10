// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash/sha3.h>

namespace noir::crypto {

hash<sha3_256>& sha3_256::init() {
  if (!ctx) {
    ctx = Keccak_HashInstance{};
  }
  Keccak_HashInitialize_SHA3_256(&*ctx);
  return *this;
}

hash<sha3_256>& sha3_256::update(std::span<const char> in) {
  if (!ctx) {
    init();
  }
  Keccak_HashUpdate(&*ctx, (BitSequence*)in.data(), in.size() * 8);
  return *this;
}

void sha3_256::final(std::span<char> out) {
  Keccak_HashFinal(&*ctx, (BitSequence*)out.data());
}

std::size_t sha3_256::digest_size() const {
  return 32;
}

} // namespace noir::crypto
