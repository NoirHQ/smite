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
  pub_key pub_key_{};
  priv_validator_type type{};

  pub_key get_pub_key() const {
    return pub_key_;
  }

  void sign_vote() {}
  void sign_proposal() {}
};

} // namespace noir::consensus
