// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <boost/pfr.hpp>
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/seq.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>

namespace noir {

template<typename T, typename = void>
struct is_foreachable : std::false_type {};

template<typename T>
struct is_foreachable<T, std::enable_if_t<std::is_class_v<T>>> : std::true_type {};

template<typename T>
constexpr bool is_foreachable_v = is_foreachable<T>::value;

template<typename T>
concept foreachable = is_foreachable_v<T>;

template<typename T, typename F>
void for_each_field(T& v, F&& f) {
  boost::pfr::for_each_field(v, f);
}

template<typename T, typename F>
void for_each_field(const T& v, F&& f) {
  boost::pfr::for_each_field(v, f);
}

} // namespace noir

#define NOIR_FOR_EACH_FIELD_IMPL(r, data, field) f(v.field);

#define NOIR_FOR_EACH_FIELD(TYPE, ...) \
  namespace noir { \
    template<typename F> \
    void for_each_field(TYPE& v, F&& f) { \
      BOOST_PP_SEQ_FOR_EACH(NOIR_FOR_EACH_FIELD_IMPL, _, \
        BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)) \
    } \
    template<typename F> \
    void for_each_field(const TYPE& v, F&& f) { \
      BOOST_PP_SEQ_FOR_EACH(NOIR_FOR_EACH_FIELD_IMPL, _, \
        BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)) \
    } \
  }

#define NOIR_FOR_EACH_FIELD_DERIVED(TYPE, BASE, ...) \
  namespace noir { \
    template<typename F> \
    void for_each_field(TYPE& v, F&& f) { \
      for_each_field((BASE&)v, f); \
      BOOST_PP_SEQ_FOR_EACH(NOIR_FOR_EACH_FIELD_IMPL, _, \
        BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)) \
    } \
    template<typename F> \
    void for_each_field(const TYPE& v, F&& f) { \
      for_each_field((const BASE&)v, f); \
      BOOST_PP_SEQ_FOR_EACH(NOIR_FOR_EACH_FIELD_IMPL, _, \
        BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)) \
    } \
  }

#define NOIR_FOR_EACH_FIELD_EMBED(TYPE, ...) \
  template<typename F> \
  void for_each_field(TYPE& v, F&& f) { \
    BOOST_PP_SEQ_FOR_EACH(NOIR_FOR_EACH_FIELD_IMPL, _, \
      BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)) \
  } \
  template<typename F> \
  void for_each_field(const TYPE& v, F&& f) { \
    BOOST_PP_SEQ_FOR_EACH(NOIR_FOR_EACH_FIELD_IMPL, _, \
      BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)) \
  }
