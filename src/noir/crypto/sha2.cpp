// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash.h>
#include <openssl/evp.h>

namespace noir::crypto {

void sha256(std::span<const char> input, std::span<char> output) {
  EVP_MD_CTX* ctx = EVP_MD_CTX_new();
  EVP_DigestInit(ctx, EVP_sha256());
  EVP_DigestUpdate(ctx, input.data(), input.size());
  EVP_DigestFinal(ctx, (unsigned char*)output.data(), nullptr);
  EVP_MD_CTX_free(ctx);
}

std::vector<char> sha256(std::span<const char> input) {
  std::vector<char> output(32);
  sha256(input, output);
  return output;
}

} // namespace noir::crypto
