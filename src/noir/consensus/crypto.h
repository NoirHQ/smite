// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/refl.h>
#include <noir/common/types/bytes.h>

namespace noir::consensus {

struct pub_key {
  bytes key;

  bool empty() const {
    return key.empty();
  }

  bytes address();

  bytes get_bytes() const {
    return key;
  }

  bool verify_signature(const bytes& msg, const bytes& sig);

  std::string get_type() const;

  friend bool operator==(const pub_key& a, const pub_key& b) {
    return a.key == b.key;
  }
};

struct priv_key {
  bytes key;

  static priv_key new_priv_key();

  bytes get_bytes() const {
    return key;
  }

  bytes sign(const bytes& msg) const;

  pub_key get_pub_key();

  std::string get_type() const;

  friend bool operator==(const priv_key& a, const priv_key& b) {
    return a.key == b.key;
  }
};

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::pub_key, key);
NOIR_REFLECT(noir::consensus::priv_key, key);
