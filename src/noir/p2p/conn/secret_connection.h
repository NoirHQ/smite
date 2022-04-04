// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once
#include <noir/p2p/types.h>

namespace noir::p2p {

template<size_t NumBytes>
struct nonce {
  bytes_n<NumBytes> bz{};
  bool overflow{}; // Indicates if an overflow occurred

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

  bytes get() {
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
  bytes key;
  bytes sig;
};

struct secret_connection {
  bytes loc_priv_key;
  bytes loc_pub_key;
  bytes loc_signature;
  bytes rem_pub_key; // other's pub key

  bytes32 loc_eph_pub;
  bytes32 loc_eph_priv;

  bytes32 rem_eph_pub; // other's ephemeral pub key

  bytes32 recv_secret;
  bytes32 send_secret;
  bytes32 chal_secret;

  nonce96 recv_nonce;
  nonce96 send_nonce;

  bool is_authorized{};

  static std::shared_ptr<secret_connection> make_secret_connection(bytes& loc_priv_key);

  std::optional<std::string> shared_eph_pub_key(bytes32& received_pub_key);

  std::optional<std::string> shared_auth_sig(auth_sig_message& received_msg);

  bytes derive_secrets(bytes32& dh_secret);
};

} // namespace noir::p2p
