// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash.h>
#include <blake2.h>
#include <openssl/evp.h>

namespace noir::crypto {

void blake2b_256(std::span<const char> in, std::span<char> out) {
  blake2b((unsigned char*)out.data(), (unsigned char*)in.data(), nullptr, 32, in.size(), 0);
}

std::vector<char> blake2b_256(std::span<const char> in) {
  std::vector<char> out(32);
  blake2b_256(in, out);
  return out;
}

} // namespace noir::crypto
