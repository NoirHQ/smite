// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/check.h>
#include <noir/crypto/hash.h>
#include <xxhash.h>

namespace noir::crypto {

namespace unsafe {
  uint64_t xxh64(std::span<const char> in) {
    return XXH64(in.data(), in.size(), 0);
  }
} // namespace unsafe

/// \cond PRIVATE
struct xxh64::xxh64_impl : public hash {
  ~xxh64_impl() {
    if (state)
      XXH64_freeState(state);
  }

  hash& init() override {
    if (!state)
      state = XXH64_createState();
    XXH64_reset(state, 0);
    return *this;
  };

  hash& update(std::span<const char> in) override {
    XXH64_update(state, in.data(), in.size());
    return *this;
  }

  void final(std::span<char> out) override {
    auto hash = XXH64_digest(state);
    std::copy((char*)&hash, (char*)&hash + sizeof(decltype(hash)), out.begin());
  }

  uint64_t final() {
    return XXH64_digest(state);
  }

  size_t digest_size() override {
    return 8;
  }

private:
  XXH64_state_t* state = nullptr;
};
/// \endcond

uint64_t xxh64::final() {
  return impl->final();
}

} // namespace noir::crypto

NOIR_CRYPTO_HASH(xxh64);
