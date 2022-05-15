// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/openssl/message_digest.h>

namespace noir::openssl {

MessageDigest::~MessageDigest() {
  if (ctx) {
    EVP_MD_CTX_free(ctx);
  }
}

void MessageDigest::init(const EVP_MD* type) {
  if (!ctx) {
    ctx = EVP_MD_CTX_new();
  }
  EVP_DigestInit(ctx, type);
}

void MessageDigest::update(BytesView in) {
  EVP_DigestUpdate(ctx, in.data(), in.size());
}

void MessageDigest::final(BytesViewMut out) {
  EVP_DigestFinal(ctx, out.data(), nullptr);
}

auto MessageDigest::digest_size(const EVP_MD* type) const -> size_t {
  return EVP_MD_size(type);
}

} // namespace noir::openssl
