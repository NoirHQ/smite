// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/hex.h>
#include <noir/common/refl.h>
#include <noir/common/types/bytes.h>

#include <fc/crypto/private_key.hpp>
#include <fc/crypto/public_key.hpp>
#include <fc/crypto/sha256.hpp>

namespace noir::consensus {

constexpr uint pub_key_size{32};

struct pub_key {
  bytes key;

  bool empty() {
    return key.empty();
  }

  bytes address() {
    // check(key.size() == pub_key_size, "pub_key: unable to derive address as key has incorrect size");
    // return {key.begin(), key.begin() + 20};
    // TODO: encode in an appropriate format; use pub_key for simplicity right now
    return key;
  }

  bool verify_signature(const bytes& msg, const bytes& sig) {
    auto sig_string = std::string(sig.begin(), sig.end());
    fc::crypto::signature signature_(sig_string);
    auto digest = fc::sha256::hash(msg); // TODO: check if this is correct
    auto recovered_pub = fc::crypto::public_key(signature_, digest);
    auto recovered_pub_bytes = fc::from_base58(recovered_pub.to_string().substr(3)); // discard EOS
    return recovered_pub_bytes == key;
  }

  friend bool operator==(const pub_key& a, const pub_key& b) {
    return a.key == b.key;
  }
};

struct priv_key {
  bytes key;

private:
  fc::crypto::private_key get_priv_key() const {
    auto base58str = fc::to_base58(key, fc::yield_function_t());
    return fc::crypto::private_key(base58str);
  }

public:
  bytes sign(const bytes& msg) {
    auto digest = fc::sha256::hash(msg); // TODO: check if this is correct
    auto priv_key_ = get_priv_key();
    auto sig_string = priv_key_.sign(digest).to_string();
    return {sig_string.begin(), sig_string.end()}; // TODO: how to store signature? signature begins with "SIG_K1_..."
  }

  pub_key get_pub_key() {
    auto priv_key_ = get_priv_key();
    pub_key pub_key_{};
    pub_key_.key = fc::from_base58(priv_key_.get_public_key().to_string().substr(3)); // discard EOS
    return pub_key_;
  }

  friend bool operator==(const priv_key& a, const priv_key& b) {
    return a.key == b.key;
  }
};

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::pub_key, key);
NOIR_REFLECT(noir::consensus::priv_key, key);
