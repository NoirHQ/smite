// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/codec/datastream.h>
#include <google/protobuf/message_lite.h>

namespace noir::codec::protobuf {

template<typename T>
constexpr size_t encode_size(const T& v) {
  return v.ByteSizeLong();
}

template<typename T>
Bytes encode(const T& v) {
  Bytes buffer(encode_size(v));
  v.SerializeToArray(buffer.data(), buffer.size());
  return buffer;
}

template<typename T>
T decode(std::span<const unsigned char> s) {
  T v{};
  v.ParseFromArray(s.data(), s.size());
  return v;
}

template<typename T>
void decode(std::span<const unsigned char> s, T& v) {
  v.ParseFromArray(s.data(), s.size());
}

} // namespace noir::codec::protobuf
