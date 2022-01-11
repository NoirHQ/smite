// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/types.h>

namespace noir::consensus {

struct priv_validator {
  bytes pub_key;

  bytes get_pub_key() {
    return pub_key;
  }

  void sign_vote();
  void sign_proposal();
};

} // namespace noir::consensus
