// This file is part of NOIR.
//
// Copyright Henry de Valence <hdevalence@hdevalence.ca>.
// License: CC0, attribution kindly requested. Blame taken too, but not liability.
//
#pragma once
#include <cstdint>
#include <cstdlib>

namespace noir::p2p {

typedef struct merlin_strobe128_ {
  union {
    uint64_t state[25];
    uint8_t state_bytes[200];
  };
  uint8_t pos;
  uint8_t pos_begin;
  uint8_t cur_flags;
} merlin_strobe128;

typedef struct merlin_transcript_ {
  merlin_strobe128 sctx;
} merlin_transcript;

void merlin_transcript_init(merlin_transcript* mctx, const uint8_t* label, size_t label_len);

void merlin_transcript_commit_bytes(
  merlin_transcript* mctx, const uint8_t* label, size_t label_len, const uint8_t* data, size_t data_len);

void merlin_transcript_challenge_bytes(
  merlin_transcript* mctx, const uint8_t* label, size_t label_len, uint8_t* buffer, size_t buffer_len);

} // namespace noir::p2p
