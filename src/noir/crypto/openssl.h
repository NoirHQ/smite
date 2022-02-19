// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash.h>
#include <openssl/evp.h>

namespace noir::openssl {

struct hash : public crypto::hash {
  hash& init() override {
    if (!ctx)
      ctx = EVP_MD_CTX_new();
    EVP_DigestInit(ctx, type());
    return *this;
  }

  hash& update(std::span<const char> in) override {
    EVP_DigestUpdate(ctx, in.data(), in.size());
    return *this;
  }

  void final(std::span<char> out) override {
    EVP_DigestFinal(ctx, (unsigned char*)out.data(), nullptr);
  }

  size_t digest_size() override {
    return EVP_MD_size(type());
  }

  virtual const EVP_MD* type() = 0;

protected:
  EVP_MD_CTX* ctx;
};

} // namespace noir::openssl
