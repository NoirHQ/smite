// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash/keccak.h>

#define Keccak_HashInitialize_Keccak256(hashInstance) Keccak_HashInitialize(hashInstance, 1088, 512, 256, 0x01)

namespace noir::crypto {

auto Keccak256::init() -> Keccak256& {
  if (!ctx) {
    ctx.emplace();
  }
  Keccak_HashInitialize_Keccak256(&*ctx);
  return *this;
}

auto Keccak256::update(BytesView in) -> Keccak256& {
  if (!ctx) {
    init();
  }
  Keccak_HashUpdate(&*ctx, (BitSequence*)in.data(), in.size() * 8);
  return *this;
}

void Keccak256::final(BytesViewMut out) {
  Keccak_HashFinal(&*ctx, (BitSequence*)out.data());
}

} // namespace noir::crypto
