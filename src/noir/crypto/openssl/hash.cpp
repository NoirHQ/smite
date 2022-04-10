// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/openssl/hash.h>

namespace noir::openssl {

hash::~hash() {
  if (ctx) {
    EVP_MD_CTX_free(ctx);
  }
}

void hash::init(const EVP_MD* type) {
  if (!ctx) {
    ctx = EVP_MD_CTX_new();
  }
  EVP_DigestInit(ctx, type);
}

void hash::update(std::span<const char> in) {
  EVP_DigestUpdate(ctx, in.data(), in.size());
}

void hash::final(std::span<char> out) {
  EVP_DigestFinal(ctx, (unsigned char*)out.data(), nullptr);
}

std::size_t hash::digest_size(const EVP_MD* type) const {
  return EVP_MD_size(type);
}

} // namespace noir::openssl
