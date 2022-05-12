// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <boost/multiprecision/cpp_int.hpp>

namespace noir {

#ifdef __SIZEOF_INT128__
using int128_t = __int128;
using uint128_t = unsigned __int128;
#else
using boost::multiprecision::int128_t;
using boost::multiprecision::uint128_t;
#endif

using boost::multiprecision::int256_t;
using boost::multiprecision::uint256_t;

template<typename DataStream, typename T>
requires(!std::is_base_of_v<std::ostream, DataStream> &&
  (std::is_same_v<T, int256_t> || std::is_same_v<T, uint256_t>)) //
void write_int256le(DataStream& ds, const T& v) {
  uint64_t data[4] = {0};
  boost::multiprecision::export_bits(v, std::begin(data), 64, false);
  ds.write((const char*)data, 32);
}

template<typename DataStream, typename T>
requires(!std::is_base_of_v<std::istream, DataStream> &&
  (std::is_same_v<T, int256_t> || std::is_same_v<T, uint256_t>)) //
void read_int256le(DataStream& ds, T& v) {
  uint64_t data[4] = {0};
  ds.read((char*)data, 32);
  boost::multiprecision::import_bits(v, std::begin(data), std::end(data), 64, false);
}

} // namespace noir
