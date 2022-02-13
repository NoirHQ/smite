// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/check.h>
#include <noir/crypto/hash.h>
#include <openssl/evp.h>

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

void ripemd160(std::span<const char> in, std::span<char> out) {
  check(out.size() >= 20, "insufficient output buffer");
  unsafe::ripemd160(in, out);
}

std::vector<char> ripemd160(std::span<const char> in) {
  std::vector<char> out(20);
  unsafe::ripemd160(in, out);
  return out;
}

} // namespace noir::crypto
