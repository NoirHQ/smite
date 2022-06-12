// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash/sha3.h>

namespace noir::crypto {

auto Sha3_256::init() -> Sha3_256& {
  if (!ctx) {
    ctx.emplace();
  }
  Keccak_HashInitialize_SHA3_256(&*ctx);
  return *this;
}

auto Sha3_256::update(BytesView in) -> Sha3_256& {
  if (!ctx) {
    init();
  }
  Keccak_HashUpdate(&*ctx, (BitSequence*)in.data(), in.size() * 8);
  return *this;
}

void Sha3_256::final(BytesViewMut out) {
  Keccak_HashFinal(&*ctx, (BitSequence*)out.data());
}

} // namespace noir::crypto
