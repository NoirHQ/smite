// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash/keccak.h>

#define Keccak_HashInitialize_Keccak256(hashInstance) Keccak_HashInitialize(hashInstance, 1088, 512, 256, 0x01)

namespace noir::crypto {

hash<keccak256>& keccak256::init() {
  if (!ctx) {
    ctx = Keccak_HashInstance{};
  }
  Keccak_HashInitialize_Keccak256(&*ctx);
  return *this;
}

hash<keccak256>& keccak256::update(std::span<const char> in) {
  if (!ctx) {
    init();
  }
  Keccak_HashUpdate(&*ctx, (BitSequence*)in.data(), in.size() * 8);
  return *this;
}

void keccak256::final(std::span<char> out) {
  Keccak_HashFinal(&*ctx, (BitSequence*)out.data());
}

std::size_t keccak256::digest_size() const {
  return 32;
}

} // namespace noir::crypto
