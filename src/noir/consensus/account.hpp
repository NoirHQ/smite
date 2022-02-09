// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/consensus/types.h>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index_container.hpp>
#include <fc/time.hpp>

namespace noir::consensus {

class account : public noir::object<account_object_type, account> {
public:
  id_type id;
  account_name name;
};
using account_id_type = account::id_type;

struct by_id;
struct by_name;
using account_index = boost::multi_index::multi_index_container<account,
  boost::multi_index::indexed_by<boost::multi_index::ordered_unique<boost::multi_index::tag<by_id>,
                                   boost::multi_index::member<account, account::id_type, &account::id>>,
    boost::multi_index::ordered_unique<boost::multi_index::tag<by_name>,
      boost::multi_index::member<account, account_name, &account::name>>>>;

class account_metadata : public noir::object<account_metadata_object_type, account_metadata> {
public:
  enum class flags_fields : uint32_t { privileged = 1 };

  id_type id;
  account_name name;
  uint64_t recv_sequence = 0;
  uint64_t auth_sequence = 0;
  uint64_t code_sequence = 0;
  uint64_t abi_sequence = 0;
  digest_type code_hash;
  fc::time_point last_code_update;
  uint32_t flags = 0;
  uint8_t vm_type = 0;
  uint8_t vm_version = 0;

  bool is_privileged() const {
    return has_field(flags, flags_fields::privileged);
  }

  void set_privileged(bool privileged) {
    flags = set_field(flags, flags_fields::privileged, privileged);
  }
};

struct by_id;
struct by_name;
using account_metadata_index = boost::multi_index::multi_index_container<account_metadata,
  boost::multi_index::indexed_by<
    boost::multi_index::ordered_unique<boost::multi_index::tag<by_id>,
      boost::multi_index::member<account_metadata, account_metadata::id_type, &account_metadata::id>>,
    boost::multi_index::ordered_unique<boost::multi_index::tag<by_name>,
      boost::multi_index::member<account_metadata, account_name, &account_metadata::name>>>>;

class account_ram_correction : public noir::object<account_ram_correction_object_type, account_ram_correction> {
public:
  id_type id;
  account_name name; //< name should not be changed within a chainbase modifier lambda
  uint64_t ram_correction = 0;
};

struct by_id;
struct by_name;
using account_ram_correction_index = boost::multi_index::multi_index_container<account_ram_correction,
  boost::multi_index::indexed_by<
    boost::multi_index::ordered_unique<boost::multi_index::tag<by_id>,
      boost::multi_index::member<account_ram_correction, account_ram_correction::id_type, &account_ram_correction::id>>,
    boost::multi_index::ordered_unique<boost::multi_index::tag<by_name>,
      boost::multi_index::member<account_ram_correction, account_name, &account_ram_correction::name>>>>;

} // namespace noir::consensus

 NOIR_SET_INDEX_TYPE(noir::consensus::account, noir::consensus::account_index)
 NOIR_SET_INDEX_TYPE(noir::consensus::account_metadata, noir::consensus::account_metadata_index)
 NOIR_SET_INDEX_TYPE(noir::consensus::account_ram_correction, noir::consensus::account_ram_correction_index)

FC_REFLECT(noir::consensus::account, (name))
FC_REFLECT(noir::consensus::account_metadata,
  (name)(recv_sequence)(auth_sequence)(code_sequence)(abi_sequence)(code_hash)(last_code_update)(flags)(vm_type)(vm_version))
FC_REFLECT(noir::consensus::account_ram_correction, (name)(ram_correction))
