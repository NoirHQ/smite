// This file is part of NOIR.
//
// Copyright 2002-2016 The OpenSSL Project Authors. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0
//
#include <noir/common/mem_clr.h>
#include <cstring>

namespace noir {

using memset_t = void* (*)(void*, int, size_t);

static volatile memset_t memset_func = memset;

void mem_cleanse(void* ptr, size_t len) {
  memset_func(ptr, 0, len);
}

} // namespace noir
