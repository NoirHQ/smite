// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/consensus/common.h>
#include <noir/consensus/crypto.h>

extern "C" {
#include <sodium.h>
}

namespace noir::consensus {

constexpr uint pub_key_size{32};
constexpr uint priv_key_size{64};
constexpr uint signature_size{64};
constexpr std::string_view key_type{"ed25519"};

namespace detail {
  Result<Bytes> sign(const Bytes& msg, const Bytes& key) {
    Bytes sig(signature_size);
    if (crypto_sign_detached(reinterpret_cast<unsigned char*>(sig.data()), nullptr,
          reinterpret_cast<const unsigned char*>(msg.data()), msg.size(),
          reinterpret_cast<const unsigned char*>(key.data())) == 0)
      return sig;
    return Error::format("unable to sign");
  }

  bool verify(const Bytes& sig, const Bytes& msg, const Bytes& key) {
    if (crypto_sign_verify_detached(reinterpret_cast<const unsigned char*>(sig.data()),
          reinterpret_cast<const unsigned char*>(msg.data()), msg.size(),
          reinterpret_cast<const unsigned char*>(key.data())) == 0)
      return true;
    return false;
  }
} // namespace detail

Bytes pub_key::address() {
  check(key.size() == pub_key_size, "pub_key: unable to derive address as key has incorrect size");
  auto h = crypto::Sha256()(key);
  return {h.begin(), h.begin() + 20};
}

bool pub_key::verify_signature(const Bytes& msg, const Bytes& sig) {
  return detail::verify(sig, msg, key);
}

std::string pub_key::get_type() const {
  return std::string(key_type);
}

priv_key priv_key::new_priv_key() {
  Bytes pub_key_(pub_key_size), priv_key_(priv_key_size);
  crypto_sign_keypair(
    reinterpret_cast<unsigned char*>(pub_key_.data()), reinterpret_cast<unsigned char*>(priv_key_.data()));
  return {.key = priv_key_};
}

Bytes priv_key::sign(const Bytes& msg) const {
  auto sig = detail::sign(msg, key);
  // TODO: what do when sign failed?
  return sig.value();
}

pub_key priv_key::get_pub_key() {
  return {.key = {key.begin() + 32, key.end()}};
}

std::string priv_key::get_type() const {
  return std::string(key_type);
}

} // namespace noir::consensus
