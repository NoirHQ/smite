// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <boost/preprocessor/if.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/preprocessor/variadic/size.hpp>
#include <boost/preprocessor/variadic/to_seq.hpp>
#include <cstddef>
#include <string_view>

/// \defgroup refl Reflection
/// \brief Type reflection

/// \brief refl namespace
/// \ingroup refl
namespace noir::refl {

/// \addtogroup refl
/// \{

/// \brief check whether T has type reflection
template<typename T>
struct has_refl : std::false_type {};

template<typename T>
inline constexpr bool has_refl_v = has_refl<T>::value;

/// \brief provides the number how many fields T has
template<typename T>
struct fields_count;

template<typename T>
inline constexpr size_t fields_count_v = fields_count<T>::value;

/// \brief field descriptor
template<size_t I, typename T>
struct field;

template<size_t I, typename T>
using field_t = typename field<I, T>::type;

/// \breif gets the field reference of T by index
template<size_t I, typename T>
field_t<I, T>& get(T&);

/// \breif gets the constant field reference of T by index
template<size_t I, typename T>
const field_t<I, T>& get(const T&);

/// \brief base class of T, when T is derived class
template<typename T>
struct base {
  using type = void;
};

template<typename T>
using base_t = typename base<T>::type;

/// \cond PRIVATE
template<size_t I, typename F, typename T>
bool _for_each_field(F&& f, T& v) {
  if constexpr (I < fields_count_v<T>) {
    if (!f(field<I, T>{}, get<I>(v))) {
      return false;
    }
    return _for_each_field<I + 1>(f, v);
  }
  return true;
}

template<size_t I, typename F, typename T>
bool _for_each_field(F&& f, const T& v) {
  if constexpr (I < fields_count_v<T>) {
    if (!f(field<I, T>{}, get<I>(v))) {
      return false;
    }
    return _for_each_field<I + 1>(f, v);
  }
  return true;
}
/// \endcond

/// \brief iterates fields of T
template<typename F, typename T>
bool for_each_field(F&& f, T& v) {
  if constexpr (!std::is_void_v<base_t<T>>) {
    if (!_for_each_field<0>(f, (base_t<T>&)v)) {
      return false;
    }
  }
  return _for_each_field<0>(f, v);
}

/// \brief iterates const fields of const T
template<typename F, typename T>
bool for_each_field(F&& f, const T& v) {
  if constexpr (!std::is_void_v<base_t<T>>) {
    if (!_for_each_field<0>(f, (const base_t<T>&)v)) {
      return false;
    }
  }
  return _for_each_field<0>(f, v);
}

/// \}

} // namespace noir::refl

namespace noir {

/// \brief helper Concepts to check whether T has reflection
template<typename T>
concept reflection = refl::has_refl_v<T>;

} // namespace noir

#define _NOIR_REFLECT_FIELD(r, TYPE, INDEX, FIELD) \
  namespace noir::refl { \
    template<> \
    struct field<INDEX, TYPE> { \
      using type = decltype(TYPE::FIELD); \
      std::string_view name = BOOST_PP_STRINGIZE(FIELD); \
      uint32_t tag = INDEX + 1; \
    }; \
  }

#define _NOIR_REFLECT_FIELD_GET(r, TYPE, INDEX, FIELD) \
  template<> \
  inline field_t<INDEX, TYPE>& get<INDEX>(TYPE & v) { \
    return v.FIELD; \
  } \
  template<> \
  inline const field_t<INDEX, TYPE>& get<INDEX>(const TYPE& v) { \
    return v.FIELD; \
  }

/// \brief helper macro for type reflection without field descriptors
#define NOIR_REFLECT_NO_DESC(TYPE, ...) \
  namespace noir::refl { \
    template<> \
    struct has_refl<TYPE> : std::true_type {}; \
    template<> \
    struct fields_count<TYPE> { \
      static constexpr size_t value = BOOST_PP_VARIADIC_SIZE(__VA_ARGS__); \
    }; \
    BOOST_PP_SEQ_FOR_EACH_I(_NOIR_REFLECT_FIELD_GET, \
      TYPE, \
      BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)) \
  }

/// \brief helper macro for type reflection with field descriptors
#define NOIR_REFLECT(TYPE, ...) \
  BOOST_PP_SEQ_FOR_EACH_I(_NOIR_REFLECT_FIELD, TYPE, \
    BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)) \
  NOIR_REFLECT_NO_DESC(TYPE, __VA_ARGS__)

/// \brief helper macro for reflection of derived type without field descriptors
#define NOIR_REFLECT_DERIVED_NO_DESC(TYPE, BASE, ...) \
  namespace noir::refl { \
    template<> \
    struct base<TYPE> { \
      using type = BASE; \
    }; \
  } \
  NOIR_REFLECT_NO_DESC(TYPE, __VA_ARGS__)

/// \brief helper macro for reflection of derived type with field descriptors
#define NOIR_REFLECT_DERIVED(TYPE, BASE, ...) \
  BOOST_PP_SEQ_FOR_EACH_I(_NOIR_REFLECT_FIELD, TYPE, \
    BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)) \
  NOIR_REFLECT_DERIVED_NO_DESC(TYPE, BASE, __VA_ARGS__)
