#pragma once

// big endian architectures need #define __BYTE_ORDER __BIG_ENDIAN
#ifndef _MSC_VER
#ifdef __APPLE__
#include <machine/endian.h>
#endif  /* __APPLE__ */
#else
#include_next <endian.h>
#endif
