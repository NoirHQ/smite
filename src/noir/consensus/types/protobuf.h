// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once
#include <noir/consensus/common.h>
#include <noir/consensus/types/validator.h>
#include <tendermint/abci/types.pb.h>

namespace noir::consensus {

struct tm2pb {
  static ::tendermint::abci::Validator to_validator(const std::shared_ptr<validator>& val) {
    ::tendermint::abci::Validator ret;
    *ret.mutable_address() = std::string(val->pub_key_.address().begin(), val->pub_key_.address().end());
    ret.set_power(val->voting_power);
    return ret;
  }

  static ::tendermint::abci::ValidatorUpdate validator_update(const std::shared_ptr<validator>& val) {
    ::tendermint::abci::ValidatorUpdate ret;
    ret.set_allocated_pub_key(pub_key::to_proto(val->pub_key_).value().release());
    ret.set_power(val->voting_power);
    return ret;
  }

  static std::vector<::tendermint::abci::ValidatorUpdate> validator_updates(
    const std::shared_ptr<validator_set>& vals) {
    std::vector<::tendermint::abci::ValidatorUpdate> ret;
    for (auto& val : vals->validators)
      ret.push_back(tm2pb::validator_update(std::make_shared<validator>(val)));
    return ret;
  }
};

struct pb2tm {
  static Result<std::vector<validator>> validator_updates(std::vector<::tendermint::abci::ValidatorUpdate>& vals) {
    std::vector<validator> tm_vals;
    for (auto& v : vals) {
      auto pub = pub_key::from_proto(v.pub_key());
      tm_vals.push_back(validator::new_validator(*pub.value(), v.power()));
    }
    return tm_vals;
  }
};

} // namespace noir::consensus
