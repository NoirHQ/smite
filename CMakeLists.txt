cmake_minimum_required(VERSION 3.16)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

project(noir)

include(CTest)
include(libs/cmake/Catch2/Catch.cmake)

set(CMAKE_CXX_STANDARD 20)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.0)
    message(FATAL_ERROR "GCC version 10 or later is required")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.0)
    message(FATAL_ERROR "Clang version 10 or later is required")
  endif()
endif()

add_compile_definitions(BOOST_ASIO_HAS_STD_INVOKE_RESULT)

if(APPLE)
  set(BOOST_ROOT "/usr/local/opt/boost" CACHE BOOL "boost root directory")
  set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl" CACHE BOOL "openssl root directory")
endif()

add_subdirectory(libs)
add_subdirectory(src)
add_subdirectory(proto)
add_subdirectory(cmd)
