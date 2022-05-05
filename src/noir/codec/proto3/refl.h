// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#include <noir/common/refl.h>
#include <boost/preprocessor/tuple/elem.hpp>

#define _PROTOBUF_REFLECT_FIELD(r, TYPE, INDEX, VAR) \
  namespace noir::refl { \
  template<> \
  struct field<INDEX, TYPE> { \
    using type = decltype(TYPE::BOOST_PP_TUPLE_ELEM(0, VAR)); \
    std::string_view name = BOOST_PP_STRINGIZE(BOOST_PP_TUPLE_ELEM(0, VAR)); \
    uint32_t tag = BOOST_PP_TUPLE_ELEM(1, VAR); \
  }; \
  }

#define _PROTOBUF_REFLECT_FIELD_GET(r, TYPE, INDEX, VAR) \
  template<> \
  inline field_t<INDEX, TYPE>& get<INDEX>(TYPE & v) { \
    return v.BOOST_PP_TUPLE_ELEM(0, VAR); \
  } \
  template<> \
  inline const field_t<INDEX, TYPE>& get<INDEX>(const TYPE& v) { \
    return v.BOOST_PP_TUPLE_ELEM(0, VAR); \
  }

#define PROTOBUF_REFLECT(TYPE, ...) \
  BOOST_PP_SEQ_FOR_EACH_I(_PROTOBUF_REFLECT_FIELD, TYPE, \
    BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)) \
  namespace noir::refl { \
  template<> \
  struct has_refl<TYPE> : std::true_type {}; \
  template<> \
  struct fields_count<TYPE> { \
    static constexpr size_t value = BOOST_PP_VARIADIC_SIZE(__VA_ARGS__); \
  }; \
  BOOST_PP_SEQ_FOR_EACH_I(_PROTOBUF_REFLECT_FIELD_GET, \
    TYPE, \
    BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)) \
  }