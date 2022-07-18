// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/helper/variant.h>
#include <noir/common/time.h>
#include <noir/consensus/types/genesis.h>

#include <cppcodec/base64_default_rfc4648.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>
#include <fmt/core.h>

#include <ctime>

namespace noir::consensus {

Result<std::shared_ptr<genesis_doc>> genesis_doc::genesis_doc_from_file(const std::string& gen_doc_file) {
  auto gen_doc = std::make_shared<genesis_doc>();
  try {
    fc::variant obj = fc::json::from_file(gen_doc_file);
    fc::from_variant(obj, *gen_doc);
    std::string dt;
    fc::from_variant(obj["genesis_time"], dt);
    if (auto ok = genesis_time_to_tstamp(dt); !ok) {
      return Error::format("error reading genesis from {}: unable to parse genesis_time", gen_doc_file);
    } else {
      gen_doc->genesis_time = ok.value();
      ilog(fmt::format("genesis_time: {}", tstamp_to_str(gen_doc->genesis_time)));
    }
    std::vector<json::genesis_validator_json_obj> vals;
    fc::from_variant(obj["validators"], vals);
    for (auto& val : vals) {
      auto addr = from_hex(val.address);
      auto pub_key_str = base64::decode(val.pub_key.value);
      ::noir::consensus::pub_key pub_key_{.key = Bytes{pub_key_str.begin(), pub_key_str.end()}};
      gen_doc->validators.push_back(genesis_validator{
        .address = Bytes{addr.begin(), addr.end()}, .pub_key = pub_key_, .power = val.power, .name = val.name});
    }
    try {
      consensus_params params;
      fc::from_variant(obj["consensus_params"], params);
      gen_doc->cs_params = params;
    } catch (std::exception const& ex) {
      dlog("genesis.json: missing consensus_params");
    }
  } catch (std::exception const& ex) {
    return Error::format("error reading genesis from {}: {}", gen_doc_file, ex.what());
  }
  return gen_doc;
}

bool genesis_doc::validate_and_complete() {
  if (chain_id.empty()) {
    elog("genesis doc must include non-empty chain_id");
    return false;
  }
  if (chain_id.length() > max_chain_id_len) {
    elog(fmt::format("chain_id in genesis doc is too long (max={})", max_chain_id_len));
    return false;
  }
  if (initial_height < 0) {
    elog("initial_height cannot be negative");
    return false;
  }
  if (initial_height == 0)
    initial_height = 1;

  if (!cs_params.has_value()) {
    cs_params = consensus_params::get_default();
  } else {
    if (auto ok = cs_params->validate_consensus_params(); !ok) {
      elog(ok.error().message());
      return false;
    }
  }

  int i{0};
  for (auto& v : validators) {
    if (v.power == 0) {
      elog("genesis file cannot contain validators with no voting power");
      return false;
    }
    // todo - uncomment after implementing methods to derive address from pub_key
    // if (!v.address.empty() && v.pub_key_.address() != v.address) {
    //  elog("genesis doc contains address that does not match its pub_key.address");
    //  return false;
    //}
    if (v.address.empty())
      validators[i].address = v.pub_key.address();
    i++;
  }

  if (genesis_time == 0)
    genesis_time = std::chrono::system_clock::now().time_since_epoch().count();
  return true;
}

std::string gen_random(const int len) {
  static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  std::string tmp_s;
  tmp_s.reserve(len);
  for (int i = 0; i < len; ++i) {
    tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
  }
  return tmp_s;
}

void genesis_doc::save(const std::string& file_path) {
  json::genesis_json_obj json_obj;
  json_obj.genesis_time = tstamp_to_genesis_str(genesis_time);

  if (chain_id.empty()) {
    chain_id = "test-chain-" + gen_random(7);
  }
  json_obj.chain_id = chain_id;

  json_obj.initial_height = to_string(initial_height);

  for (const auto& validator : validators) {
    json::genesis_validator_json_obj val;
    val.address = to_string(validator.address);
    val.pub_key.value = base64::encode(validator.pub_key.key.data(), validator.pub_key.key.size());
    val.power = validator.power;
    val.name = validator.name;
    json_obj.validators.push_back(val);
  }

  if (!cs_params.has_value())
    cs_params = consensus_params::get_default();
  json_obj.consensus_params = cs_params.value();

  json_obj.app_hash = to_string(app_hash);
  json_obj.app_state = to_string(app_state);

  fc::variant vo;
  fc::to_variant<json::genesis_json_obj>(json_obj, vo);
  fc::json::save_to_file(vo, file_path);
}

} // namespace noir::consensus
