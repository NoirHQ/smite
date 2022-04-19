// This file is part of NOIR.
//
// Copyright (c) 2010 Respective Authors
// SPDX-License-Identifier: MIT
//
#include <noir/thread/thread_name.h>
#include <pthread.h>

static thread_local std::string thread_name;

namespace noir::thread {

const std::string& thread_name() {
  if (::thread_name.empty()) {
    char name[64];
    if (pthread_getname_np(pthread_self(), name, 64) == 0) {
      ::thread_name = name;
    }
    static int thread_count = 0;
    ::thread_name = "thread-" + std::to_string(thread_count++);
  }
  return ::thread_name;
}

void set_os_thread_name(const std::string& name) {
#if defined(__linux__)
  pthread_setname_np(pthread_self(), name.c_str());
#elif defined(__APPLE__)
  pthread_setname_np(name.c_str());
#endif
}

void set_thread_name(const std::string& name) {
  ::thread_name = name;
}

} // namespace noir::thread
