cmake_minimum_required(VERSION 3.16)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

project(smite)

option(NOIR_BUILD_TESTS "Build tests" ON)
option(NOIR_UNITY_BUILD "Enable unity build" OFF)
option(USE_CONAN "Use conan to setup build dependencies" ON)
option(CHECK_CONNECTION "Check the network connection status" ON)

if(NOIR_BUILD_TESTS)
  include(CTest)
  include(libs/cmake/Catch2/Catch.cmake)
endif()

set(CMAKE_CXX_STANDARD 20)
add_compile_definitions(BOOST_ASIO_HAS_STD_INVOKE_RESULT)

include(cmake_common.txt)

compiler_minimum_required("GNU" 11.0)
compiler_minimum_required("Clang" 10.0)
compiler_minimum_required("AppleClang" 10.0)

setup_conan()
setup_macos_path()

set(Boost_USE_STATIC_LIBS ON)

find_package(Catch2)
find_package(Boost REQUIRED)
find_package(cppcodec REQUIRED)
find_package(fmt REQUIRED)
find_package(libb2 REQUIRED)
find_package(libpqxx REQUIRED)
find_package(OpenSSL 3 REQUIRED)
find_package(Protobuf REQUIRED)
find_package(RocksDB REQUIRED)
find_package(scope-lite REQUIRED)
find_package(spdlog REQUIRED)
find_package(xxHash REQUIRED)
find_package(date REQUIRED)

add_subdirectory(libs)
add_subdirectory(src)
add_subdirectory(proto)
add_subdirectory(cmd)
