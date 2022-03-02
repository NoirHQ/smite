// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <noir/common/refl.h>
#include <fc/variant_object.hpp>

namespace noir {

template<reflection T>
void to_variant(const T& in, fc::variant& out) {
  fc::mutable_variant_object obj;
  refl::for_each_field([&](const auto& name, const auto& value) { obj.set(std::string(name), value); }, in);
  out = obj;
}

template<reflection T>
void from_variant(const fc::variant& in, T& out) {
  check(in.is_object());
  auto obj = in.get_object();
  refl::for_each_field([&](const auto& name, auto& value) { from_variant(obj[std::string(name)], value); }, out);
}

} // namespace noir
