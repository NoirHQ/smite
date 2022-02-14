// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <eth/common/types.h>
#include <fc/variant.hpp>

namespace eth::api {

class api {
public:
  static void check_send_raw_tx(const fc::variant& req);
  std::string send_raw_tx(const std::string& rlp, const uint256_t& tx_fee_cap, const bool allow_unprotected_txs);
};

} // namespace eth::api
