// This file is part of NOIR.
//
// Copyright (c) 2017-2021 block.one and its contributors.  All rights reserved.
// SPDX-License-Identifier: MIT
//
#pragma once
#include <noir/log/log.h>

namespace tendermint::net::details {

template<typename Strand>
void verify_strand_in_this_thread(const Strand& strand, const char* func, int line) {
  if (!strand.running_in_this_thread()) {
    elog("wrong strand: {} : line {}, exiting", func, line);
    // app.quit();
  }
}

} // namespace tendermint::net::details
