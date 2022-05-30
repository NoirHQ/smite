// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/helper/rust.h>
#include <noir/common/refl.h>
#include <noir/common/types/bytes.h>
#include <tendermint/crypto/keys.pb.h>

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

  static result<std::unique_ptr<::tendermint::crypto::PublicKey>> to_proto(const pub_key& p) {
    auto ret = std::make_unique<::tendermint::crypto::PublicKey>();
    if (p.get_type() == "ed25519")
      ret->set_ed25519({p.get_bytes().begin(), p.get_bytes().end()});
    else
      return make_unexpected(fmt::format("to_proto failed: key_type '{}' is not supported", p.get_type()));
    return ret;
  }

  static result<std::shared_ptr<pub_key>> from_proto(const ::tendermint::crypto::PublicKey& pb) {
    auto ret = std::make_shared<pub_key>();
    if (!pb.has_ed25519())
      return make_unexpected("only ed25519 is supported");
    ret->key = {pb.ed25519().begin(), pb.ed25519().end()};
    return ret;
  }

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
