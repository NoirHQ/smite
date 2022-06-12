// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/bytes.h>
#include <noir/common/check.h>
#include <noir/common/concepts.h>
#include <noir/common/hex.h>
#include <noir/common/refl.h>
#include <fc/reflect/variant.hpp>

namespace fc {

inline void to_variant(const int64_t& in, variant& out) {
  out = in;
}

inline void to_variant(const uint64_t& in, variant& out) {
  out = in;
}

template<size_t N>
void to_variant(const noir::BytesN<N>& in, variant& out) {
  out = in.to_string();
}

template<size_t N>
void from_variant(const variant& in, noir::BytesN<N>& out) {
  out = {in.as_string()};
}

inline void to_variant(const bool& in, variant& out) {
  out = in;
}

template<noir::Enumeration E>
void to_variant(const E& in, variant& out) {
  to_variant(static_cast<std::underlying_type_t<E>>(in), out);
}

template<noir::Enumeration E>
void from_variant(const variant& in, E& out) {
  std::underlying_type_t<E> tmp;
  from_variant(in, tmp);
  out = static_cast<E>(tmp);
}

template<noir::Reflected T>
void to_variant(const T& in, variant& out) {
  mutable_variant_object obj;
  noir::refl::for_each_field(
    [&](const auto& desc, const auto& value) {
      fc::variant var;
      to_variant(value, var);
      obj.set(std::string(desc.name), var);
      return true;
    },
    in);
  out = obj;
}

template<noir::Reflected T>
void from_variant(const variant& in, T& out) {
  noir::check(in.is_object());
  auto obj = in.get_object();
  noir::refl::for_each_field(
    [&](const auto& desc, auto& value) {
      from_variant(obj[std::string(desc.name)], value);
      return true;
    },
    out);
}

} // namespace fc
