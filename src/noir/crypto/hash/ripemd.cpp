// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash/ripemd.h>
#include <noir/crypto/openssl/openssl.h>

namespace noir::crypto {

crypto::hash<ripemd160>& ripemd160::init() {
  openssl::init();
  openssl::hash::init(EVP_ripemd160());
  return *this;
}

crypto::hash<ripemd160>& ripemd160::update(std::span<const char> in) {
  if (!ctx) {
    init();
  }
  openssl::hash::update(in);
  return *this;
}

void ripemd160::final(std::span<char> out) {
  openssl::hash::final(out);
}

std::size_t ripemd160::digest_size() const {
  return openssl::hash::digest_size(EVP_ripemd160());
}

} // namespace noir::crypto
