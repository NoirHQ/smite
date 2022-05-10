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
  static ::tendermint::abci::Validator to_validator(std::shared_ptr<validator> val) {
    ::tendermint::abci::Validator ret;
    *ret.mutable_address() = std::string(val->pub_key_.address().begin(), val->pub_key_.address().end());
    ret.set_power(val->voting_power);
    return ret;
  }
};

} // namespace noir::consensus
