// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <spdlog/spdlog.h>
#include <string>

namespace noir::log {

void set_level(const std::string& level);

void setup(spdlog::logger* logger = nullptr);

} // namespace noir::log
