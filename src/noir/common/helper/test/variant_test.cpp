// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <catch2/catch_all.hpp>
#include <noir/common/concepts.h>
#include <noir/common/refl.h>

struct signed_integral_type {
  int8_t a = 0xf;
  int16_t b = 0xff;
  int32_t c = 0xffff;
  int64_t d = 0xffffffffll;

  bool operator==(const signed_integral_type& o) const {
    return (a == o.a) && (b == o.b) && (c == o.c) && (d == o.d);
  }
};
NOIR_REFLECT(signed_integral_type, a, b, c, d)

struct unsigned_integral_type {
  uint8_t a = 0xf;
  uint16_t b = 0xff;
  uint32_t c = 0xffff;
  uint64_t d = 0xffffffffll;

  bool operator==(const unsigned_integral_type& o) const {
    return (a == o.a) && (b == o.b) && (c == o.c) && (d == o.d);
  }
};
NOIR_REFLECT(unsigned_integral_type, a, b, c, d)

struct complex_integral_type {
  signed_integral_type a;
  signed_integral_type b;
  unsigned_integral_type c;
  unsigned_integral_type d;

  bool operator==(const complex_integral_type& o) const {
    return (a == o.a) && (b == o.b) && (c == o.c) && (d == o.d);
  }
};
NOIR_REFLECT(complex_integral_type, a, b, c, d)

enum class signed_enum_8 : int8_t {
  // workaround clang-format
  a = 0xf,
};
enum class signed_enum_16 : int16_t {
  // workaround clang-format
  b = 0xff,
};
enum class signed_enum_32 : int32_t {
  // workaround clang-format
  c = 0xffff,
};
enum class signed_enum_64 : int64_t {
  // workaround clang-format
  d = 0xffffffffll,
};
struct signed_enumeration_type {
  signed_enum_8 a = signed_enum_8::a;
  signed_enum_16 b = signed_enum_16::b;
  signed_enum_32 c = signed_enum_32::c;
  signed_enum_64 d = signed_enum_64::d;

  bool operator==(const signed_enumeration_type& o) const {
    return (a == o.a) && (b == o.b) && (c == o.c) && (d == o.d);
  }
};
NOIR_REFLECT(signed_enumeration_type, a, b, c, d)

enum class unsigned_enum_8 : int8_t {
  // workaround clang-format
  a = 0xf,
};
enum class unsigned_enum_16 : int16_t {
  // workaround clang-format
  b = 0xff,
};
enum class unsigned_enum_32 : int32_t {
  // workaround clang-format
  c = 0xffff,
};
enum class unsigned_enum_64 : int64_t {
  // workaround clang-format
  d = 0xffffffffll,
};
struct unsigned_enumeration_type {
  unsigned_enum_8 a = unsigned_enum_8::a;
  unsigned_enum_16 b = unsigned_enum_16::b;
  unsigned_enum_32 c = unsigned_enum_32::c;
  unsigned_enum_64 d = unsigned_enum_64::d;

  bool operator==(const unsigned_enumeration_type& o) const {
    return (a == o.a) && (b == o.b) && (c == o.c) && (d == o.d);
  }
};
NOIR_REFLECT(unsigned_enumeration_type, a, b, c, d)

struct complex_enumeration_type {
  signed_enumeration_type a;
  signed_enumeration_type b;
  unsigned_enumeration_type c;
  unsigned_enumeration_type d;

  bool operator==(const complex_enumeration_type& o) const {
    return (a == o.a) && (b == o.b) && (c == o.c) && (d == o.d);
  }
};
NOIR_REFLECT(complex_enumeration_type, a, b, c, d)

struct nested_complex_type {
  complex_integral_type a;
  complex_enumeration_type b;
  complex_integral_type c;
  complex_integral_type d;

  bool operator==(const nested_complex_type& o) const {
    return (a == o.a) && (b == o.b) && (c == o.c) && (d == o.d);
  }
};
NOIR_REFLECT(nested_complex_type, a, b, c, d)

struct vector_integral_type {
  std::vector<int8_t> a;
  std::vector<int16_t> b;
  std::vector<int32_t> c;
  std::vector<int64_t> d;

  vector_integral_type() {
    a.resize(10);
    b.resize(10);
    c.resize(10);
    d.resize(10);
  }
  bool operator==(const vector_integral_type& o) const {
    return (a == o.a) && (b == o.b) && (c == o.c) && (d == o.d);
  }
};
NOIR_REFLECT(vector_integral_type, a, b, c, d);

struct vector_enumeration_type {
  std::vector<signed_enum_8> a;
  std::vector<signed_enum_16> b;
  std::vector<signed_enum_32> c;
  std::vector<signed_enum_64> d;

  vector_enumeration_type() {
    a.resize(10);
    b.resize(10);
    c.resize(10);
    d.resize(10);
  }
  bool operator==(const vector_enumeration_type& o) const {
    return (a == o.a) && (b == o.b) && (c == o.c) && (d == o.d);
  }
};
NOIR_REFLECT(vector_enumeration_type, a, b, c, d);

struct complex_vector_type {
  std::vector<complex_integral_type> a;
  std::vector<complex_enumeration_type> b;
  std::vector<vector_integral_type> c;
  std::vector<vector_enumeration_type> d;

  complex_vector_type() {
    a.resize(10);
    b.resize(10);
    c.resize(10);
    d.resize(10);
  }
  bool operator==(const complex_vector_type& o) const {
    return (a == o.a) && (b == o.b) && (c == o.c) && (d == o.d);
  }
};
NOIR_REFLECT(complex_vector_type, a, b, c, d)

#include <noir/common/helper/variant.h>

namespace {

TEMPLATE_TEST_CASE("variant: to/from_variant", "[noir][common]", signed_integral_type, unsigned_integral_type,
  complex_integral_type, signed_enumeration_type, unsigned_enumeration_type, complex_enumeration_type,
  nested_complex_type, vector_enumeration_type, complex_vector_type) {

  TestType exp{};

  fc::variant tmp;
  TestType ret;
  fc::to_variant(exp, tmp);
  fc::from_variant(tmp, ret);
  CHECK(exp == ret);
}

} // namespace
