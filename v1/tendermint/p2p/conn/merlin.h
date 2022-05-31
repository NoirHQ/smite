// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <cstdint>
#include <cstdlib>

namespace tendermint::p2p::conn {

struct MerlinStrobe128 {
  union {
    uint64_t state[25];
    uint8_t state_bytes[200];
  };
  uint8_t pos;
  uint8_t pos_begin;
  uint8_t cur_flags;
};

struct MerlinTranscript {
  MerlinStrobe128 sctx;
};

void merlin_transcript_init(MerlinTranscript* mctx, const uint8_t* label, size_t label_len);

void merlin_transcript_commit_bytes(
  MerlinTranscript* mctx, const uint8_t* label, size_t label_len, const uint8_t* data, size_t data_len);

void merlin_transcript_challenge_bytes(
  MerlinTranscript* mctx, const uint8_t* label, size_t label_len, uint8_t* buffer, size_t buffer_len);

} //namespace tendermint::p2p::conn
