// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
//#include <noir/codec/protobuf.h>
#include <noir/codec/bcs.h>

namespace noir {

// using codec::proto3::datastream;
using codec::bcs::datastream;

template<typename T>
constexpr size_t encode_size(const T& v) {
  datastream<size_t> ds;
  ds << v;
  return ds.tellp();
}

template<typename T>
Bytes encode(const T& v) {
  Bytes buffer(encode_size(v));
  datastream<unsigned char> ds(buffer);
  ds << v;
  return buffer;
}

template<typename T>
T decode(std::span<const unsigned char> s) {
  T v;
  datastream<const unsigned char> ds(s);
  ds >> v;
  return v;
}

} // namespace noir
