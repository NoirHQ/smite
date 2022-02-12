// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash.h>
#include <blake2.h>
#include <openssl/evp.h>

namespace noir::crypto {

void blake2b_256(std::span<const char> input, std::span<char> output) {
  blake2b((unsigned char*)output.data(), (unsigned char*)input.data(), nullptr, 32, input.size(), 0);
}

std::vector<char> blake2b_256(std::span<const char> input) {
  std::vector<char> output(32);
  blake2b_256(input, output);
  return output;
}

} // namespace noir::crypto
