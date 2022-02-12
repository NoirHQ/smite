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
add_subdirectory(websocketpp)

# use cached rocksdb in github build workflow
set(ROCKSDB_CACHED ${CMAKE_CURRENT_BINARY_DIR}/rocksdb/librocksdb.a)
if(NOT TARGET rocksdb AND EXISTS ${ROCKSDB_CACHED})
  add_library(rocksdb STATIC IMPORTED GLOBAL)
  set_target_properties(rocksdb PROPERTIES IMPORTED_LOCATION ${ROCKSDB_CACHED})
  target_include_directories(rocksdb INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/rocksdb/include)
  set(rocksdb_FOUND TRUE)
endif()

if(NOT rocksdb_FOUND)
  # override rocksdb cmake options
  set(PORTABLE ON)
  set(ROCKSDB_BUILD_SHARED OFF)
  set(WITH_TESTS OFF)
  set(WITH_BENCHMARK_TOOLS OFF)
  set(WITH_CORE_TOOLS OFF)
  set(WITH_TOOLS OFF)

  add_subdirectory(rocksdb)
  target_include_directories(rocksdb INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/rocksdb/include>)
endif()

set(XKCP_ARCH ${CMAKE_CURRENT_SOURCE_DIR}/XKCP/bin/generic64)
# suppress non-existent include error
file(MAKE_DIRECTORY ${XKCP_ARCH}/libXKCP.a.headers)
# workaround fix to github linux build workflow
if(NOT APPLE)
  add_custom_command(
    OUTPUT ${XKCP_ARCH}/libXKCP.a
    COMMAND CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} make generic64/libXKCP.a
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/XKCP)
else()
  add_custom_command(
    OUTPUT ${XKCP_ARCH}/libXKCP.a
    COMMAND make generic64/libXKCP.a
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/XKCP)
endif()
add_custom_target(XKCP_build DEPENDS ${XKCP_ARCH}/libXKCP.a)

add_library(XKCP_include INTERFACE)
target_include_directories(XKCP_include INTERFACE ${XKCP_ARCH}/libXKCP.a.headers)

add_library(XKCP STATIC IMPORTED GLOBAL)
set_target_properties(XKCP PROPERTIES IMPORTED_LOCATION ${XKCP_ARCH}/libXKCP.a)
add_dependencies(XKCP XKCP_build)
target_link_libraries(XKCP INTERFACE XKCP_include)

find_library(B2_LIBRARY libb2.a)
if(NOT B2_LIBRARY STREQUAL B2_LIBRARY-NOTFOUND)
  find_path(B2_INCLUDE_DIR blake2.h)

  add_library(b2 STATIC IMPORTED)
  set_target_properties(b2 PROPERTIES IMPORTED_LOCATION ${B2_LIBRARY})
  target_include_directories(b2 INTERFACE ${B2_INCLUDE_DIR})
endif()
