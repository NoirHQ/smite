// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/bytes.h>
#include <noir/common/refl.h>
#include <noir/core/result.h>
#include <tendermint/crypto/keys.pb.h>

namespace noir::consensus {

struct pub_key {
  Bytes key;

  bool empty() const {
    return key.empty();
  }

  Bytes address();

  Bytes get_bytes() const {
    return key;
  }

  bool verify_signature(const Bytes& msg, const Bytes& sig);

  std::string get_type() const;

  static Result<std::unique_ptr<::tendermint::crypto::PublicKey>> to_proto(const pub_key& p) {
    auto ret = std::make_unique<::tendermint::crypto::PublicKey>();
    if (p.get_type() == "ed25519") {
      Bytes bz = p.get_bytes();
      ret->set_ed25519({bz.begin(), bz.end()});
    } else {
      return Error::format("to_proto failed: key_type '{}' is not supported", p.get_type());
    }
    return ret;
  }

  static Result<std::shared_ptr<pub_key>> from_proto(const ::tendermint::crypto::PublicKey& pb) {
    auto ret = std::make_shared<pub_key>();
    ret->key = {pb.ed25519().begin(), pb.ed25519().end()};
    if (ret->key.empty())
      return Error::format("only ed25519 is supported");
    return ret;
  }

  friend bool operator==(const pub_key& a, const pub_key& b) {
    return a.key == b.key;
  }
};

struct priv_key {
  Bytes key;

  static priv_key new_priv_key();

  Bytes get_bytes() const {
    return key;
  }

  Bytes sign(const Bytes& msg) const;

  pub_key get_pub_key();

  std::string get_type() const;

  friend bool operator==(const priv_key& a, const priv_key& b) {
    return a.key == b.key;
  }
};

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::pub_key, key);
NOIR_REFLECT(noir::consensus::priv_key, key);
