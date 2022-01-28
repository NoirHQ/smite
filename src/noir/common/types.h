// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/for_each.h>
#include <fc/crypto/ripemd160.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/io/varint.hpp>
#include <span>

namespace noir {

using ::fc::signed_int;
using ::fc::unsigned_int;

struct bytes20 : fc::ripemd160 {
  using ripemd160::ripemd160;
};

template<>
struct is_foreachable<bytes20> : std::false_type {};

template<typename DataStream>
DataStream& operator<<(DataStream& ds, const bytes20& v) {
  ds << std::span((const char*)v._hash, 20);
  return ds;
}

template<typename DataStream>
DataStream& operator>>(DataStream& ds, bytes20& v) {
  ds >> std::span((char*)v._hash, 20);
  return ds;
}

struct bytes32 : fc::sha256 {
  using sha256::sha256;
};

template<>
struct is_foreachable<bytes32> : std::false_type {};

template<typename DataStream>
DataStream& operator<<(DataStream& ds, const bytes32& v) {
  ds << std::span((const char*)v._hash, 32);
  return ds;
}

template<typename DataStream>
DataStream& operator>>(DataStream& ds, bytes32& v) {
  ds >> std::span((char*)v._hash, 32);
  return ds;
}

} // namespace noir
