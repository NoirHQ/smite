// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/rand.h>
#include <openssl/rand.h>
#include <stdexcept>

namespace noir::crypto {

void rand_bytes(std::span<char> out) {
  if (!RAND_bytes((unsigned char*)out.data(), out.size())) {
    throw std::runtime_error("failed to generate random bytes");
  }
}

void rand_priv_bytes(std::span<char> out) {
  if (!RAND_priv_bytes((unsigned char*)out.data(), out.size())) {
    throw std::runtime_error("failed to generate random bytes");
  }
}

} // namespace noir::crypto
