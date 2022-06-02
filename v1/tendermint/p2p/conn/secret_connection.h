// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/types.h>
#include <noir/core/core.h>

namespace tendermint::p2p::conn {
template<size_t NumBytes>
struct Nonce {
  noir::BytesN<NumBytes> bz{};
  bool overflow{}; // Indicates if an overflow occurred

  /// Not thread safe
  void increment() {
    unsigned int carry = 1;
    for (int64_t i = bz.size() - 1; carry > 0; --i) {
      unsigned int current = *reinterpret_cast<unsigned char*>(&bz[i]);
      current += carry;
      *reinterpret_cast<unsigned char*>(&bz[i]) = current & 0xff;
      carry = current >> 8;
    }
    if (carry > 0)
      overflow = true;
  }

  noir::Bytes get() {
    return bz.to_bytes();
  }

  constexpr size_t size() {
    return bz.size();
  }

  bool operator==(const Nonce<NumBytes>& other) {
    return bz == other.bz and overflow == other.overflow;
  }
};
typedef Nonce<12> nonce96;

struct AuthSigMessage {
  noir::Bytes key;
  noir::Bytes sig;
};

class SecretConnection {
public:
  bool is_authorized{};
  static std::shared_ptr<SecretConnection> make_secret_connection(noir::Bytes& loc_priv_key);
  std::optional<std::string> shared_eph_pub_key(noir::Bytes32& received_pub_key);
  std::optional<std::string> shared_auth_sig(AuthSigMessage& received_msg);
  noir::Bytes derive_secrets(noir::Bytes32& dh_secret);

public:
  noir::Bytes loc_pub_key;
  noir::Bytes loc_signature;

  noir::Bytes32 loc_eph_pub;
  noir::Bytes32 recv_secret;
  noir::Bytes32 send_secret;
  noir::Bytes32 chal_secret;

private:
  noir::Bytes loc_priv_key;
  noir::Bytes rem_pub_key; // other's pub key

  noir::Bytes32 loc_eph_priv;
  noir::Bytes32 rem_eph_pub; // other's ephemeral pub key

  nonce96 recv_nonce;
  nonce96 send_nonce;
};

} //namespace noir::p2p::conn
