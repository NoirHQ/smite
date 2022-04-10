// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash/sha2.h>

namespace noir::crypto {

crypto::hash<sha256>& sha256::init() {
  openssl::hash::init(EVP_sha256());
  return *this;
}

crypto::hash<sha256>& sha256::update(std::span<const char> in) {
  if (!ctx) {
    init();
  }
  openssl::hash::update(in);
  return *this;
}

void sha256::final(std::span<char> out) {
  openssl::hash::final(out);
}

std::size_t sha256::digest_size() const {
  return openssl::hash::digest_size(EVP_sha256());
}

} // namespace noir::crypto
