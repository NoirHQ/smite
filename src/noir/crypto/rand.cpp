// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/check.h>
#include <noir/crypto/rand.h>
#include <openssl/rand.h>

namespace noir::crypto {

void rand_bytes(std::span<byte_type> out) {
  check(RAND_bytes((unsigned char*)out.data(), out.size()), "failed to generate random bytes");
}

void rand_priv_bytes(std::span<byte_type> out) {
  check(RAND_priv_bytes((unsigned char*)out.data(), out.size()), "failed to generate random bytes");
}

} // namespace noir::crypto
