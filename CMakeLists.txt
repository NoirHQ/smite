cmake_minimum_required(VERSION 3.16)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

project(noir)

option(NOIR_BUILD_TESTS "Build tests" OFF)
option(NOIR_UNITY_BUILD "Enable unity build" OFF)

if(NOIR_BUILD_TESTS)
  include(CTest)
  include(libs/cmake/Catch2/Catch.cmake)
endif()

set(CMAKE_CXX_STANDARD 20)
add_compile_definitions(BOOST_ASIO_HAS_STD_INVOKE_RESULT)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.0)
    message(FATAL_ERROR "GCC version 10 or later is required")
  endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 10.0)
    message(FATAL_ERROR "Clang version 10 or later is required")
  endif()
endif()

#########################################
# Install build dependencies with conan #
#########################################
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_BINARY_DIR})
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_BINARY_DIR})

if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/conan.cmake")
  message(STATUS "Downloading conan.cmake from https://github.com/conan-io/cmake-conan")
  file(DOWNLOAD "https://github.com/conan-io/cmake-conan/raw/0.18.1/conan.cmake" "${CMAKE_CURRENT_BINARY_DIR}/conan.cmake" TLS_VERIFY ON)
endif()

include(${CMAKE_CURRENT_BINARY_DIR}/conan.cmake)

set(CONAN_PACKAGES
  fmt/8.1.1
  libb2/20190723
  libpqxx/7.7.3
  rocksdb/6.20.3
  scope-lite/0.2.0
  spdlog/1.10.0
  xxhash/0.8.1
  date/3.0.1
)

if(UNIX AND NOT APPLE)
  set(CONAN_PACKAGES
    ${CONAN_PACKAGES}
    boost/1.78.0
    openssl/3.0.2
  )
endif()

conan_cmake_configure(REQUIRES ${CONAN_PACKAGES} GENERATORS cmake_find_package)

set(NOIR_BUILD_TYPE ${CMAKE_BUILD_TYPE})
set(CMAKE_BUILD_TYPE Release)
conan_cmake_autodetect(CONAN_SETTINGS)
set(CMAKE_BUILD_TYPE ${NOIR_BUILD_TYPE})

# HACK: remove c++ standard version from conan settings to use prebuilt binaries
string(REPLACE ";compiler.cppstd=20" "" CONAN_SETTINGS "${CONAN_SETTINGS}")
conan_cmake_install(PATH_OR_REFERENCE . BUILD missing REMOTE conancenter SETTINGS ${CONAN_SETTINGS})

if(APPLE)
  if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(BREW_ROOT "/usr/local/opt")
  elseif(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(BREW_ROOT "/opt/homebrew/opt")
  endif()
  set(BOOST_ROOT "${BREW_ROOT}/boost" CACHE STRING "boost root directory")
  set(OPENSSL_ROOT_DIR "${BREW_ROOT}/openssl" CACHE STRING "openssl root directory")
endif()

find_package(Boost REQUIRED)
find_package(fmt REQUIRED)
find_package(libb2 REQUIRED)
find_package(libpqxx REQUIRED)
find_package(OpenSSL 3 REQUIRED)
find_package(RocksDB REQUIRED)
find_package(scope-lite REQUIRED)
find_package(spdlog REQUIRED)
find_package(xxHash REQUIRED)
find_package(date REQUIRED)

add_subdirectory(libs)
add_subdirectory(src)
add_subdirectory(proto)
add_subdirectory(proto_legacy)
add_subdirectory(cmd)
