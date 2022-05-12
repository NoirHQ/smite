// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/helper/variant.h>
#include <noir/consensus/types/genesis.h>

#include <fc/crypto/base64.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>
#include <fmt/core.h>

#include <ctime>

namespace noir::consensus {

std::shared_ptr<genesis_doc> genesis_doc::genesis_doc_from_file(const std::string& gen_doc_file) {
  auto gen_doc = std::make_shared<genesis_doc>();
  try {
    fc::variant obj = fc::json::from_file(gen_doc_file);
    fc::from_variant(obj, *gen_doc);
    std::string dt;
    fc::from_variant(obj["genesis_time"], dt);
    struct tm tm_info {};
    if (strptime(dt.c_str(), "%Y-%m-%dT%H:%M:%S", &tm_info)) {
      std::time_t tt = std::mktime(&tm_info);
      std::chrono::system_clock::time_point tp = std::chrono::system_clock::from_time_t(tt);
      gen_doc->genesis_time = tp.time_since_epoch().count();
    } else {
      elog(fmt::format("error reading genesis from {}: unable to parse genesis_time", gen_doc_file));
      return {};
    }
    std::vector<json::genesis_validator_json_obj> vals;
    fc::from_variant(obj["validators"], vals);
    for (auto& val : vals) {
      auto addr = from_hex(val.address);
      auto pub_key_str = fc::base64_decode(val.pub_key.value);
      ::noir::consensus::pub_key pub_key_{.key = bytes{pub_key_str.begin(), pub_key_str.end()}};
      gen_doc->validators.push_back(
        {.address = bytes{addr.begin(), addr.end()}, .pub_key = pub_key_, .power = val.power, .name = val.name});
    }
  } catch (std::exception const& ex) {
    elog(fmt::format("error reading genesis from {}: {}", gen_doc_file, ex.what()));
    return {};
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
    auto err = cs_params->validate_consensus_params();
    if (err.has_value()) {
      elog(err.value());
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

} // namespace noir::consensus