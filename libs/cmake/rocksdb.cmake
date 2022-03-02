if(USE_CACHED_LIBS)
  add_cached_library(rocksdb ${CMAKE_CURRENT_BINARY_DIR}/rocksdb/librocksdb.a
    INTERFACE
      ${CMAKE_CURRENT_SOURCE_DIR}/rocksdb/include)
endif()

if(NOT TARGET rocksdb)
  find_package(rocksdb)
  if(rocksdb_FOUND)
    set_target_properties(RocksDB::rocksdb PROPERTIES IMPORTED_GLOBAL TRUE)
    add_library(rocksdb ALIAS RocksDB::rocksdb)
  endif()
endif()

if(NOT TARGET rocksdb)
  set(PORTABLE ON)
  set(ROCKSDB_BUILD_SHARED OFF)
  set(WITH_TESTS OFF)
  set(WITH_BENCHMARK_TOOLS OFF)
  set(WITH_CORE_TOOLS OFF)
  set(WITH_TOOLS OFF)

  add_subdirectory(rocksdb)
  target_include_directories(rocksdb INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/rocksdb/include>)
endif()

