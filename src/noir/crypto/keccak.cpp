// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash.h>

extern "C" {
#include <KeccakHash.h>
}

#define Keccak_HashInitialize_Keccak256(hashInstance) Keccak_HashInitialize(hashInstance, 1088, 512, 256, 0x01)

namespace noir::crypto {

void keccak256(std::span<const char> input, std::span<char> output) {
  Keccak_HashInstance ctx;
  Keccak_HashInitialize_Keccak256(&ctx);
  Keccak_HashUpdate(&ctx, (BitSequence*)input.data(), input.size() * 8);
  Keccak_HashFinal(&ctx, (BitSequence*)output.data());
}

std::vector<char> keccak256(std::span<const char> input) {
  std::vector<char> output(32);
  keccak256(input, output);
  return output;
}

} // namespace noir::crypto