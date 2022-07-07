// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/bytes.h>
#include <noir/crypto/hash/sha2.h>
#include <tendermint/types/mempool.h>

// FIXME: put this under tendermint later
namespace noir::types {

struct Tx : public Bytes {
  TxKey key() const {
    return crypto::Sha256()(std::span{data(), size()});
  }
};

} // namespace noir::types
