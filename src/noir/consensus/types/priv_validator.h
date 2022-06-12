// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/common.h>
#include <noir/consensus/types/vote.h>

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
  virtual priv_validator_type get_type() const = 0;
  virtual pub_key get_pub_key() const = 0;
  virtual priv_key get_priv_key() const = 0;
  virtual std::optional<std::string> sign_vote(vote& vote_) = 0;
  virtual std::optional<std::string> sign_proposal(noir::p2p::proposal_message& proposal_) = 0;
  virtual Result<Bytes> sign_vote_pb(const std::string& chain_id, const ::tendermint::types::Vote& v) = 0;
};

struct mock_pv : public priv_validator {
  pub_key pub_key_{};
  priv_key priv_key_{};

  priv_validator_type get_type() const override {
    return priv_validator_type::MockSignerClient;
  }
  pub_key get_pub_key() const override {
    return pub_key_;
  }
  priv_key get_priv_key() const override {
    return priv_key_;
  }
  std::optional<std::string> sign_vote(vote& vote_) override;
  std::optional<std::string> sign_proposal(noir::p2p::proposal_message& proposal_) override;
  Result<Bytes> sign_vote_pb(const std::string& chain_id, const ::tendermint::types::Vote& v) override;
};

} // namespace noir::consensus
