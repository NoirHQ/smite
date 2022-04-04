// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <tendermint/log/log.h>
#include <tendermint/net/details/verify_strand.h>

// peer_[x]log must be called from thread in connection strand
#define peer_dlog(PEER, FORMAT, ...) \
  do { \
    tendermint::net::details::verify_strand_in_this_thread(PEER->strand, __func__, __LINE__); \
    dlog(FORMAT, __VA_ARGS__); \
  } while (0)

#define peer_ilog(PEER, FORMAT, ...) \
  do { \
    tendermint::net::details::verify_strand_in_this_thread(PEER->strand, __func__, __LINE__); \
    ilog(FORMAT, __VA_ARGS__); \
  } while (0)

#define peer_wlog(PEER, FORMAT, ...) \
  do { \
    tendermint::net::details::verify_strand_in_this_thread(PEER->strand, __func__, __LINE__); \
    wlog(FORMAT, __VA_ARGS__); \
  } while (0)

#define peer_elog(PEER, FORMAT, ...) \
  do { \
    tendermint::net::details::verify_strand_in_this_thread(PEER->strand, __func__, __LINE__); \
    elog(FORMAT, __VA_ARGS__); \
  } while (0)
