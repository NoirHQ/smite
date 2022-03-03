// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <noir/common/concepts.h>
#include <noir/common/refl.h>
#include <fc/reflect/variant.hpp>

namespace fc {

template<noir::enumeration E>
void to_variant(const E& in, variant& out) {
  auto tmp = static_cast<std::underlying_type_t<E>>(in);
  if constexpr (requires(const E& in) { variant(tmp); }) {
    out = tmp;
  } else {
    to_variant(tmp, out);
  }
}

template<noir::reflection T>
void to_variant(const T& in, variant& out) {
  mutable_variant_object obj;
  noir::refl::for_each_field(
    [&](const auto& name, const auto& value) { obj.set(std::string(name), variant{value}); }, in);
  out = obj;
}

template<noir::enumeration E>
void from_variant(const variant& in, E& out) {
  std::underlying_type_t<E> tmp;
  from_variant(in, tmp);
  out = static_cast<E>(tmp);
}

template<noir::reflection T>
void from_variant(const variant& in, T& out) {
  noir::check(in.is_object());
  auto obj = in.get_object();
  noir::refl::for_each_field([&](const auto& name, auto& value) { from_variant(obj[std::string(name)], value); }, out);
}

} // namespace fc
