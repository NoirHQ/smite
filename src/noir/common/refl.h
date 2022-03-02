// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/check.h>
#include <noir/common/types/fixed_string.h>
#include <boost/preprocessor/if.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/variadic/size.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
#include <cstddef>
#include <stdexcept>
#include <string_view>

namespace noir::refl {

template<typename T>
struct has_refl : std::false_type {};

template<typename T>
inline constexpr bool has_refl_v = has_refl<T>::value;

template<typename T>
struct fields_count;

template<typename T>
inline constexpr size_t fields_count_v = fields_count<T>::value;

template<size_t I, typename T>
struct field;

template<size_t I, typename T>
using field_t = typename field<I, T>::type;

template<size_t I, typename T>
field_t<I, T>& get(T&);

template<size_t I, typename T>
const field_t<I, T>& get(const T&);

template<typename T>
struct base {
  using type = void;
};

template<typename T>
using base_t = typename base<T>::type;

template<size_t I, typename F, typename T>
void for_each_field_impl(F&& f, T& v) {
  if constexpr (I < fields_count_v<T>) {
    f(std::string_view(field<I, T>::name), get<I>(v));
    for_each_field_impl<I + 1>(f, v);
  }
}

template<typename F, typename T>
void for_each_field(F&& f, T& v) {
  if constexpr (!std::is_void_v<base_t<T>>) {
    for_each_field_impl<0>(f, (base_t<T>&)v);
  }
  for_each_field_impl<0>(f, v);
}

template<size_t I, typename F, typename T>
void for_each_field_impl(F&& f, const T& v) {
  if constexpr (I < fields_count_v<T>) {
    f(std::string_view(field<I, T>::name), get<I>(v));
    for_each_field_impl<I + 1>(f, v);
  }
}

template<typename F, typename T>
void for_each_field(F&& f, const T& v) {
  if constexpr (!std::is_void_v<base_t<T>>) {
    for_each_field_impl<0>(f, (const base_t<T>&)v);
  }
  for_each_field_impl<0>(f, v);
}

} // namespace noir::refl

namespace noir {

template<typename T>
concept reflection = refl::has_refl_v<T>;

} // namespace noir

#define NOIR_REFLECT_IMPL(r, TYPE, INDEX, FIELD) \
  template<> \
  struct field<INDEX, TYPE> { \
    using type = decltype(TYPE::FIELD); \
    static constexpr fixed_string name = BOOST_PP_STRINGIZE(FIELD); \
  }; \
  template<> \
  inline field_t<INDEX, TYPE>& get<INDEX>(TYPE & v) { \
    return v.FIELD; \
  } \
  template<> \
  inline const field_t<INDEX, TYPE>& get<INDEX>(const TYPE& v) { \
    return v.FIELD; \
  }

#define NOIR_REFLECT(TYPE, ...) \
  namespace noir::refl { \
    template<> \
    struct has_refl<TYPE> : std::true_type {}; \
    template<> \
    struct fields_count<TYPE> { \
      static constexpr size_t value = BOOST_PP_VARIADIC_SIZE(__VA_ARGS__); \
    }; \
    BOOST_PP_SEQ_FOR_EACH_I(NOIR_REFLECT_IMPL, TYPE, \
      BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)) \
  }

#define NOIR_REFLECT_DERIVED(TYPE, BASE, ...) \
  namespace noir::refl { \
    template<> \
    struct has_refl<TYPE> : std::true_type {}; \
    template<> \
    struct base<TYPE> { \
      using type = BASE; \
    }; \
    template<> \
    struct fields_count<TYPE> { \
      static constexpr size_t value = BOOST_PP_VARIADIC_SIZE(__VA_ARGS__); \
    }; \
    BOOST_PP_SEQ_FOR_EACH_I(NOIR_REFLECT_IMPL, TYPE, \
      BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)) \
  }
