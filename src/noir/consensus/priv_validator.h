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

  priv_key priv_key_{}; // TODO: temp fix; need to implement FilePV, which reads private key from a file

  pub_key get_pub_key() const {
    return pub_key_;
  }

  std::optional<std::string> sign_vote(vote& vote_);
  std::optional<std::string> sign_proposal(proposal& proposal_);
};

} // namespace noir::consensus
