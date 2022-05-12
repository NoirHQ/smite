// This file is part of NOIR.
//
// Copyright (c) 2022 Haderech Pte. Ltd.
// SPDX-License-Identifier: AGPL-3.0-or-later
//
#pragma once
#include <noir/common/foreachable.h>
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
struct HasRefl : std::false_type {};

template<typename T>
inline constexpr bool has_refl_v = HasRefl<T>::value;

/// \brief provides the number how many fields T has
template<typename T>
struct FieldsCount;

template<typename T>
inline constexpr size_t fields_count_v = FieldsCount<T>::value;

/// \brief field descriptor
template<size_t I, typename T>
struct Field;

template<size_t I, typename T>
using FieldT = typename Field<I, T>::type;

/// \breif gets the field reference of T by index
template<size_t I, typename T>
FieldT<I, T>& get(T&);

/// \breif gets the constant field reference of T by index
template<size_t I, typename T>
const FieldT<I, T>& get(const T&);

/// \brief base class of T, when T is derived class
template<typename T>
struct Base;

template<typename T>
using BaseT = typename Base<T>::type;

/// \brief iterates fields of T
template<size_t I = 0, typename F, typename T>
bool for_each_field(F&& f, T& v) {
  if constexpr (!I && !std::is_void_v<BaseT<T>>) {
    if (!for_each_field(f, (BaseT<T>&)v)) {
      return false;
    }
  }
  if constexpr (I < fields_count_v<T>) {
    if (!f(Field<I, T>{}, get<I>(v))) {
      return false;
    }
    return for_each_field<I + 1>(f, v);
  }
  return true;
}

/// \brief iterates const fields of const T
template<size_t I = 0, typename F, typename T>
bool for_each_field(F&& f, const T& v) {
  if constexpr (!I && !std::is_void_v<BaseT<T>>) {
    if (!for_each_field(f, (const BaseT<T>&)v)) {
      return false;
    }
  }
  if constexpr (I < fields_count_v<T>) {
    if (!f(Field<I, T>{}, get<I>(v))) {
      return false;
    }
    return for_each_field<I + 1>(f, v);
  }
  return true;
}

/// \}

} // namespace noir::refl

namespace noir {

/// \brief helper Concepts to check whether T has reflection
template<typename T>
concept Reflected = refl::has_refl_v<T>;

} // namespace noir

#define NOIR_REFLECT_FIELD(r, TYPE, INDEX, FIELD) \
  namespace noir::refl { \
    template<> \
    struct Field<INDEX, TYPE> { \
      using type = decltype(TYPE::FIELD); \
      std::string_view name = BOOST_PP_STRINGIZE(FIELD); \
      uint32_t tag = (INDEX + 1); \
    }; \
  }

#define NOIR_REFLECT_FIELD_GET(r, TYPE, INDEX, FIELD) \
  namespace noir::refl { \
    template<> \
    inline FieldT<INDEX, TYPE>& get<INDEX>(TYPE & v) { \
      return v.FIELD; \
    } \
    template<> \
    inline const FieldT<INDEX, TYPE>& get<INDEX>(const TYPE& v) { \
      return v.FIELD; \
    } \
  }

#define NOIR_VARIADIC_TO_SEQ(...) \
  BOOST_PP_IF(BOOST_PP_VARIADIC_SIZE(__VA_ARGS__), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__), BOOST_PP_SEQ_NIL)

/// \brief helper macro for reflection of derived type without field descriptors
#define NOIR_REFLECT_DERIVED_NO_DESC(TYPE, BASE, ...) \
  namespace noir { \
    template<> \
    struct IsForeachable<TYPE> : std::true_type {}; \
  } \
  namespace noir::refl { \
    template<> \
    struct HasRefl<TYPE> : std::true_type {}; \
    template<> \
    struct FieldsCount<TYPE> : std::integral_constant<size_t, BOOST_PP_VARIADIC_SIZE(__VA_ARGS__)> {}; \
    template<> \
    struct Base<TYPE> { \
      using type = BASE; \
    }; \
  } \
  BOOST_PP_SEQ_FOR_EACH_I(NOIR_REFLECT_FIELD_GET, TYPE, NOIR_VARIADIC_TO_SEQ(__VA_ARGS__))

/// \brief helper macro for reflection of derived type with field descriptors
#define NOIR_REFLECT_DERIVED(TYPE, BASE, ...) \
  BOOST_PP_SEQ_FOR_EACH_I(NOIR_REFLECT_FIELD, TYPE, NOIR_VARIADIC_TO_SEQ(__VA_ARGS__)) \
  NOIR_REFLECT_DERIVED_NO_DESC(TYPE, BASE, __VA_ARGS__)

/// \brief helper macro for type reflection without field descriptors
#define NOIR_REFLECT_NO_DESC(TYPE, ...) NOIR_REFLECT_DERIVED_NO_DESC(TYPE, void, __VA_ARGS__)

/// \brief helper macro for type reflection with field descriptors
#define NOIR_REFLECT(TYPE, ...) \
  BOOST_PP_SEQ_FOR_EACH_I(NOIR_REFLECT_FIELD, TYPE, NOIR_VARIADIC_TO_SEQ(__VA_ARGS__)) \
  NOIR_REFLECT_NO_DESC(TYPE, __VA_ARGS__)
