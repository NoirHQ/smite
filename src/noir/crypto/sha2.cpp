// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash.h>
#include <openssl/evp.h>

namespace noir::crypto {

void sha256(std::span<const char> in, std::span<char> out) {
  EVP_Digest(in.data(), in.size(), (unsigned char*)out.data(), nullptr, EVP_sha256(), nullptr);
}

std::vector<char> sha256(std::span<const char> in) {
  std::vector<char> out(32);
  sha256(in, out);
  return out;
}

} // namespace noir::crypto
