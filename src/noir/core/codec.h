// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/codec/bcs.h>

namespace noir {

using codec::bcs::datastream;

template<typename T>
constexpr size_t encode_size(const T& v) {
  datastream<size_t> ds;
  ds << v;
  return ds.tellp();
}
template<typename T>
std::vector<char> encode(const T& v) {
  auto buffer = std::vector<char>(encode_size(v));
  datastream<char> ds(buffer);
  ds << v;
  return buffer;
}
template<typename T>
T decode(std::span<const char> s) {
  T v{};
  datastream<const char> ds(s);
  ds >> v;
  return v;
}

} // namespace noir
