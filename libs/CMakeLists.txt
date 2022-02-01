# Don't add the duplicated cmake target "Catch2" by expected
set(EXPECTED_BUILD_TESTS OFF)
set(EXPECTED_BUILD_PACKAGE_DEB OFF)

set(SKIP_FC_TESTS ON)

add_definitions(-DWEBSOCKETPP_STRICT_MASKING -DBOOST_ASIO_HAS_STD_CHRONO)

add_library(websocketpp INTERFACE)
target_include_directories(websocketpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/websocketpp)

add_subdirectory(softfloat)
add_subdirectory(appbase)
add_subdirectory(Catch2)
add_subdirectory(expected)
add_subdirectory(fc)
add_subdirectory(fmt)
add_subdirectory(tendermint)
add_subdirectory(websocketpp)

if(NOT rocksdb_FOUND)
  # override rocksdb cmake options
  set(ROCKSDB_BUILD_SHARED OFF)
  set(WITH_TESTS OFF)
  set(WITH_BENCHMARK_TOOLS OFF)
  set(WITH_CORE_TOOLS OFF)
  set(WITH_TOOLS OFF)

  add_subdirectory(rocksdb)
  target_include_directories(rocksdb INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/rocksdb/include>)
endif()
