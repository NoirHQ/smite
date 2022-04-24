// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/error.h>

namespace noir {

using namespace BOOST_OUTCOME_V2_NAMESPACE;

// NOTE:
// `std::expected` doesn't support construction from Error type,
// so it needs to wrap an error like `return std::unexpected(E);` to propagate it.
// `Result<T, E>` derived from `boost::outcome_v2::result<T, E>` allows construction from error.
//
// However, `boost::outcome_v2::result` doesn't have a default constructor by its design,
// so it cannot be used as a return type of `boost::asio::awaitable<R>` function.
// `boost::asio::co_spawn` calls completion token with `void(std::exception_ptr, R>` and
// `R` must be default constructible.
template<typename T, typename E = Error, typename NoValuePolicy = policy::default_policy<T, E, void>>
class Result : public basic_result<T, E, NoValuePolicy> {
public:
  using basic_result<T, E, NoValuePolicy>::basic_result;

  Result(): basic_result<T, E, NoValuePolicy>(T()) {}

  template<typename U>
  Result(std::in_place_type_t<U> _): basic_result<T, E, NoValuePolicy>(_) {}
};

template<typename E, typename NoValuePolicy>
class Result<void, E, NoValuePolicy> : public basic_result<void, E, NoValuePolicy> {
public:
  using basic_result<void, E, NoValuePolicy>::basic_result;

  Result(): basic_result<void, E, NoValuePolicy>(std::in_place_type<void>) {}
};

} // namespace noir
