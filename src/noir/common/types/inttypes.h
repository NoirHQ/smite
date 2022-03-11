// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/for_each.h>
#include <boost/multiprecision/cpp_int.hpp>
#include <span>

namespace noir {

#if defined(__SIZEOF_INT128__)
using int128_t = __int128;
using uint128_t = unsigned __int128;
#else
using int128_t = boost::multiprecision::int128_t;
using uint128_t = boost::multiprecision::uint128_t;
template<>
struct is_foreachable<int128_t> : std::false_type {};
template<>
struct is_foreachable<uint128_t> : std::false_type {};
#endif

using int256_t = boost::multiprecision::int256_t;

template<>
struct is_foreachable<int256_t> : std::false_type {};

// XXX: disabled due to compile error with gcc
//
// template<typename DataStream>
// DataStream& operator<<(DataStream& ds, const int256_t& v) {
//   uint64_t data[4] = {0};
//   boost::multiprecision::export_bits(v, std::begin(data), 64, false);
//   ds << std::span((const char*)data, 32);
//   return ds;
// }
//
// template<typename DataStream>
// DataStream& operator>>(DataStream& ds, int256_t& v) {
//   uint64_t data[4] = {0};
//   ds >> std::span((char*)data, 32);
//   boost::multiprecision::import_bits(v, std::begin(data), std::end(data), 64, false);
//   return ds;
// }

using uint256_t = boost::multiprecision::uint256_t;

template<>
struct is_foreachable<uint256_t> : std::false_type {};

// XXX: disabled due to compile error with gcc
//
// template<typename DataStream>
// DataStream& operator<<(DataStream& ds, const uint256_t& v) {
//   uint64_t data[4] = {0};
//   boost::multiprecision::export_bits(v, std::begin(data), 64, false);
//   ds << std::span((const char*)data, 32);
//   return ds;
// }
//
// template<typename DataStream>
// DataStream& operator>>(DataStream& ds, uint256_t& v) {
//   uint64_t data[4] = {0};
//   ds >> std::span((char*)data, 32);
//   boost::multiprecision::import_bits(v, std::begin(data), std::end(data), 64, false);
//   return ds;
// }

} // namespace noir
