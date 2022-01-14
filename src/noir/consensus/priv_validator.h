// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/types.h>

namespace noir::consensus {

enum priv_validator_type {
  MockSignerClient = 0,
  FileSignerClient = 1,
  RetrySignerClient = 2,
  SignerSocketClient = 3,
  ErrorMockSignerClient = 4,
  SignerGRPCClient = 5
};

struct priv_validator {
  p2p::bytes pub_key;
  priv_validator_type type;

  p2p::bytes get_pub_key() {
    return pub_key;
  }

  void sign_vote();
  void sign_proposal();
};

} // namespace noir::consensus
