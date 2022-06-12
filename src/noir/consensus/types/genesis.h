// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/refl.h>
#include <noir/consensus/crypto.h>
#include <noir/consensus/protocol.h>
#include <noir/consensus/types/params.h>
#include <noir/p2p/protocol.h>
#include <noir/p2p/types.h>

namespace noir::consensus {

constexpr size_t max_chain_id_len{50};

namespace json {
  struct key_json_obj {
    std::string type;
    std::string value;
  };
  struct genesis_validator_json_obj {
    std::string address;
    key_json_obj pub_key;
    int64_t power;
    std::string name;
  };

  struct genesis_json_obj {
    std::string genesis_time;
    std::string chain_id;
    std::string initial_height;
    std::vector<genesis_validator_json_obj> validators;
    std::string app_hash;
    std::string app_state;
  };
} // namespace json

struct genesis_validator {
  Bytes address;
  ::noir::consensus::pub_key pub_key;
  int64_t power;
  std::string name;
};

struct genesis_doc {
  tstamp genesis_time;
  std::string chain_id;
  int64_t initial_height;
  std::optional<consensus_params> cs_params;
  std::vector<genesis_validator> validators;
  Bytes app_hash;
  Bytes app_state;

  static std::shared_ptr<genesis_doc> genesis_doc_from_file(const std::string& gen_doc_file);

  void save(const std::string& file_path);
  bool validate_and_complete();
};

} // namespace noir::consensus

NOIR_REFLECT(noir::consensus::genesis_validator, address, pub_key, power, name);
NOIR_REFLECT(noir::consensus::genesis_doc, chain_id, initial_height, /*cs_params, validators,*/ app_hash, app_state);
NOIR_REFLECT(noir::consensus::json::key_json_obj, type, value);
NOIR_REFLECT(noir::consensus::json::genesis_validator_json_obj, address, pub_key, power, name);
NOIR_REFLECT(noir::consensus::json::genesis_json_obj, genesis_time, chain_id, initial_height, validators, app_hash, app_state);
