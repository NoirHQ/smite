# Don't add the duplicated cmake target "Catch2" by expected
set(EXPECTED_BUILD_TESTS OFF)
set(EXPECTED_BUILD_PACKAGE_DEB OFF)

set(SKIP_FC_TESTS ON)

set(ROCKSDB_BUILD_SHARED OFF)
set(WITH_TESTS OFF)
set(WITH_BENCHMARK_TOOLS OFF)
set(WITH_CORE_TOOLS OFF)
set(WITH_TOOLS OFF)

add_subdirectory(softfloat)

add_subdirectory(appbase)
add_subdirectory(Catch2)
add_subdirectory(expected)
add_subdirectory(fc)
add_subdirectory(fmt)
add_subdirectory(rocksdb)
add_subdirectory(tendermint)

target_include_directories(rocksdb INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/rocksdb/include>)
