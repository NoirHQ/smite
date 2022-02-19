// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/check.h>
#include <noir/crypto/openssl.h>

namespace noir::openssl {
extern void init();
} // namespace noir::openssl

namespace noir::crypto {

namespace unsafe {
  void ripemd160(std::span<const char> in, std::span<char> out) {
    openssl::init();
    EVP_Digest(in.data(), in.size(), (unsigned char*)out.data(), nullptr, EVP_ripemd160(), nullptr);
  }
} // namespace unsafe

struct ripemd160::ripemd160_impl : public openssl::hash {
  hash& init() override {
    openssl::init();
    openssl::hash::init();
    return *this;
  }

  const EVP_MD* type() override {
    return EVP_ripemd160();
  }
};

} // namespace noir::crypto

NOIR_CRYPTO_HASH(ripemd160);
