// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/crypto/hash/hash.h>
#include <blake2.h>
#include <optional>

namespace noir::crypto {

/// \brief generates blake2b_256 hash
/// \ingroup crypto
struct blake2b_256 : public hash<blake2b_256> {
  using hash::final;

  hash<blake2b_256>& init();
  hash<blake2b_256>& update(std::span<const char> in);
  void final(std::span<char> out);
  std::size_t digest_size() const;

private:
  std::optional<blake2b_state> state;
};

} // namespace noir::crypto
