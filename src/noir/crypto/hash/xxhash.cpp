// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/crypto/hash/xxhash.h>

namespace noir::crypto {

xxh64::~xxh64() {
  if (state) {
    XXH64_freeState(state);
  }
}

hash<xxh64>& xxh64::init() {
  if (!state) {
    state = XXH64_createState();
  }
  XXH64_reset(state, 0);
  return *this;
}

hash<xxh64>& xxh64::update(std::span<const char> in) {
  if (!state) {
    init();
  }
  XXH64_update(state, in.data(), in.size());
  return *this;
}

void xxh64::final(std::span<char> out) {
  auto hash = XXH64_digest(state);
  std::copy((char*)&hash, (char*)&hash + sizeof(decltype(hash)), out.begin());
}

uint64_t xxh64::final() {
  return XXH64_digest(state);
}

std::size_t xxh64::digest_size() const {
  return 8;
}

} // namespace noir::crypto
