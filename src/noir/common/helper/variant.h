// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <noir/common/concepts.h>
#include <noir/common/refl.h>
#include <fc/variant_object.hpp>

namespace noir {

template<typename T>
void to_named_variant(const T& in, const auto& name, fc::variant& out) {
  out = fc::variant{in};
}

template<enumeration E>
void to_named_variant(const E& in, const auto& name, fc::variant& out) {
  out = fc::variant{static_cast<std::underlying_type_t<E>>(in)};
}

template<reflection T>
void to_named_variant(const T& in, const auto& name, fc::variant& out) {
  fc::mutable_variant_object obj;
  refl::for_each_field(
    [&](const auto& name_, const auto& value) {
      fc::variant var;
      noir::to_named_variant(value, name_, var);
      obj.set(std::string(name_), var);
    },
    in);
  out = obj;
}

template<reflection T>
void to_variant(const T& in, fc::variant& out) {
  fc::mutable_variant_object obj;
  refl::for_each_field(
    [&](const auto& name, const auto& value) {
      fc::variant var;
      noir::to_named_variant(value, name, var);
      obj.set(std::string(name), var);
    },
    in);
  out = obj;
}

template<typename T>
void from_named_variant(const fc::variant& in, const auto& name, T& out) {
  check(!in.is_object());
  from_variant(in, out);
}

template<enumeration E>
void from_named_variant(const fc::variant& in, const auto& name, E& out) {
  check(!in.is_object());
  std::underlying_type_t<E> tmp;
  from_variant(in, tmp);
  out = static_cast<E>(tmp);
}

template<reflection T>
void from_named_variant(const fc::variant& in, const auto& name, T& out) {
  check(in.is_object());
  auto obj = in.get_object();
  refl::for_each_field(
    [&](const auto& name_, auto& value) { from_named_variant(obj[std::string(name_)], name_, value); }, out);
}

template<reflection T>
void from_variant(const fc::variant& in, T& out) {
  check(in.is_object());
  auto obj = in.get_object();
  refl::for_each_field(
    [&](const auto& name, auto& value) { from_named_variant(obj[std::string(name)], name, value); }, out);
}

} // namespace noir
