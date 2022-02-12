// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash.h>
#include <openssl/evp.h>

namespace noir::crypto {

void sha256(std::span<const char> input, std::span<char> output) {
  EVP_Digest(input.data(), input.size(), (unsigned char*)output.data(), nullptr, EVP_sha256(), nullptr);
}

std::vector<char> sha256(std::span<const char> input) {
  std::vector<char> output(32);
  sha256(input, output);
  return output;
}

} // namespace noir::crypto
