// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once

#include <cstdlib>
#include <execinfo.h>
#include <unistd.h>

namespace noir {

void print_backtrace(int sig) {
  void* array[20];

  // get void*'s for all entries on the stack
  int size = backtrace(array, 20);

  // print out all the frames to stderr
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

} // namespace noir
