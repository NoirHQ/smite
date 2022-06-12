// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/crypto/hash/hash.h>
#include <optional>

namespace noir::crypto {

/// \brief generates ripemd160 hash
/// \ingroup crypto
struct Ripemd160 : public Hash<Ripemd160> {
  using Hash::final;
  using Hash::update;

  auto init() -> Ripemd160&;
  auto update(BytesView in) -> Ripemd160&;
  void final(BytesViewMut out);

  constexpr auto digest_size() const -> size_t {
    return 20;
  }

private:
  uint32_t s[5];
  unsigned char buf[64];
  uint64_t bytes;

  bool inited{false};
};

} // namespace noir::crypto
