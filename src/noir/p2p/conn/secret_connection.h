// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/p2p/types.h>
#include <optional>

namespace noir::p2p {

template<size_t NumBytes>
struct nonce {
  BytesN<NumBytes> bz{};
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

  Bytes get() {
    return bz.to_bytes();
  }

  constexpr size_t size() {
    return bz.size();
  }

  bool operator==(const nonce<NumBytes>& other) {
    return bz == other.bz and overflow == other.overflow;
  }
};
typedef nonce<12> nonce96;

struct auth_sig_message {
  Bytes key;
  Bytes sig;
};

struct secret_connection {
  Bytes loc_priv_key;
  Bytes loc_pub_key;
  Bytes loc_signature;
  Bytes rem_pub_key; // other's pub key

  Bytes32 loc_eph_pub;
  Bytes32 loc_eph_priv;
  Bytes32 rem_eph_pub; // other's ephemeral pub key

  Bytes32 recv_secret;
  Bytes32 send_secret;
  Bytes32 chal_secret;

  nonce96 recv_nonce;
  nonce96 send_nonce;

  bool is_authorized{};

  static std::shared_ptr<secret_connection> make_secret_connection(Bytes& loc_priv_key);

  std::optional<std::string> shared_eph_pub_key(Bytes32& received_pub_key);

  std::optional<std::string> shared_auth_sig(auth_sig_message& received_msg);

  Bytes derive_secrets(Bytes32& dh_secret);
};

} // namespace noir::p2p
