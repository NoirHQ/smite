// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/core/basic.h>

#include <noir/core/as_result.h>
#include <noir/core/channel.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/io_context.hpp>

namespace noir {

using namespace boost;
using namespace boost::asio::experimental::awaitable_operators;

} //namespace noir