// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

// XXX: This header is not the official one provided by Boost.Asio,
// but adapted from `boost::asio::experimental::as_single`.
#include <boost/asio/experimental/as_result.hpp>

namespace noir {

template<typename CompletionToken>
inline auto as_result(CompletionToken&& token) {
  return boost::asio::experimental::as_result(std::forward<CompletionToken>(token));
}

} // namespace noir
