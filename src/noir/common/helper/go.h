// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/scope_exit.h>
#include <boost/preprocessor/cat.hpp>

/// \brief [go] analogous defer keyword
/// \ingroup common
#define noir_defer(...) auto BOOST_PP_CAT(_defer_, __COUNTER__) = noir::make_scope_exit(__VA_ARGS__)
