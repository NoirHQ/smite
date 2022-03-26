// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/refl.h>
#include <noir/common/types/bytes.h>
#include <noir/common/types/varint.h>
#include <optional>

namespace noir::codec::proto3 {

// static constexpr uint32_t max_tag = std::numeric_limits<uint32_t>::max() >> 3;
static constexpr uint32_t max_tag = 256;

template<typename T>
requires(std::is_signed_v<T>) struct sint : public varint<T> {
  using varint<T>::varint;
};

using sint32 = sint<int32_t>;
using sint64 = sint<int64_t>;

template<typename T>
struct fixed : public varint<T> {
  using varint<T>::varint;
};

using fixed32 = fixed<uint32_t>;
using fixed64 = fixed<uint64_t>;
using sfixed32 = fixed<int32_t>;
using sfixed64 = fixed<int64_t>;

template<typename T>
struct wire_type;

template<typename T>
static constexpr auto wire_type_v = wire_type<T>::value;

template<>
struct wire_type<int32_t> {
  static constexpr uint32_t value = 0;
};
template<>
struct wire_type<int64_t> {
  static constexpr uint32_t value = 0;
};
template<>
struct wire_type<uint32_t> {
  static constexpr uint32_t value = 0;
};
template<>
struct wire_type<uint64_t> {
  static constexpr uint32_t value = 0;
};
template<>
struct wire_type<sint32> {
  static constexpr uint32_t value = 0;
};
template<>
struct wire_type<sint64> {
  static constexpr uint32_t value = 0;
};
template<>
struct wire_type<bool> {
  static constexpr uint32_t value = 0;
};
template<typename T>
requires(std::is_enum_v<T>) struct wire_type<T> {
  static constexpr uint32_t value = 0;
};

template<>
struct wire_type<fixed64> {
  static constexpr uint32_t value = 1;
};
template<>
struct wire_type<sfixed64> {
  static constexpr uint32_t value = 1;
};
template<>
struct wire_type<double> {
  static constexpr uint32_t value = 1;
};

template<>
struct wire_type<std::string> {
  static constexpr uint32_t value = 2;
};
template<>
struct wire_type<bytes> {
  static constexpr uint32_t value = 2;
};
// embedded message
template<reflection T>
struct wire_type<T> {
  static constexpr uint32_t value = 2;
};
// packed
template<typename T>
struct wire_type<std::vector<T>> {
  static constexpr uint32_t value = 2;
};

template<>
struct wire_type<fixed32> {
  static constexpr uint32_t value = 5;
};
template<>
struct wire_type<sfixed32> {
  static constexpr uint32_t value = 5;
};
template<>
struct wire_type<float> {
  static constexpr uint32_t value = 5;
};

template<typename T>
struct wire_type<std::optional<T>> {
  static constexpr uint32_t value = wire_type_v<T>;
};

} // namespace noir::codec::proto3
