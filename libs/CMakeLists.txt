include(cmake/CachedLibs.cmake)

# add subdirectories if cached libs not found
add_subdirectory_if(softfloat NOT TARGET softfloat)
add_subdirectory_if(appbase NOT TARGET appbase)
add_subdirectory_if(Catch2 NOT TARGET Catch2)

add_definitions(-DWEBSOCKETPP_STRICT_MASKING -DBOOST_ASIO_HAS_STD_CHRONO)
add_library(websocketpp INTERFACE)
target_include_directories(websocketpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/websocketpp)

set(EXPECTED_BUILD_TESTS OFF) # Don't add the duplicated cmake target "Catch2" by expected
set(EXPECTED_BUILD_PACKAGE_DEB OFF)
add_subdirectory(expected)

set(SKIP_FC_TESTS ON)
add_subdirectory(fc)

add_subdirectory(fmt)
add_subdirectory(scope-lite)
add_subdirectory(range-v3)
add_subdirectory(xxHash/cmake_unofficial)

include(cmake/rocksdb.cmake)
include(cmake/b2.cmake)
include(cmake/XKCP.cmake)
