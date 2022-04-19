// This file is part of NOIR.
//
// Copyright (c) 2010 Respective Authors
// SPDX-License-Identifier: MIT
//
#pragma once
#include <string>

namespace noir::thread {

const std::string& thread_name();
void set_os_thread_name(const std::string& name);
void set_thread_name(const std::string& name);

} // namespace noir::thread
