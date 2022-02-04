// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/types.h>

namespace noir::consensus {

struct event_attribute {
  std::string key;
  std::string value;
  bool index;
};

struct event {
  std::string type;
  std::vector<event_attribute> attributes;
};

struct validator_update {
  // pubkey
  int64_t power;
};

struct response_deliver_tx {
  uint32_t code;
  bytes data;
  std::string log;
  std::string info;
  int64_t gas_wanted;
  int64_t gas_used;
  std::vector<event> events;
  std::string codespace;
};

struct response_begin_block {
  std::vector<event> events;
};

struct response_end_block {
  std::vector<validator_update> validator_updates;
  consensus_params consensus_param_updates;
  std::vector<event> events;
};

struct abci_responses {
  std::vector<response_deliver_tx> deliver_txs;
  response_end_block end_block;
  response_begin_block begin_block;
};

} // namespace noir::consensus
