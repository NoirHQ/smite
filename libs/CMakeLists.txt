set(CMAKE_BUILD_TYPE Release)

include(cmake/CachedLibs.cmake)

# add subdirectories if cached libs not found
add_subdirectory_if(softfloat NOT TARGET softfloat)
add_subdirectory_if(appbase NOT TARGET appbase)
add_subdirectory_if(Catch2 NOT TARGET Catch2)

# websocketpp in conan requires the specific version of boost and other dependencies.
# Let's include as header-only library here.
add_definitions(-DWEBSOCKETPP_STRICT_MASKING -DBOOST_ASIO_HAS_STD_CHRONO)
add_library(websocketpp INTERFACE)
target_include_directories(websocketpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/websocketpp)

# expected in conan doesn't support expected<void> yet
set(EXPECTED_BUILD_TESTS OFF) # Don't add the duplicated cmake target "Catch2" by expected
set(EXPECTED_BUILD_PACKAGE_DEB OFF)
add_subdirectory(expected)

# TODO: remove fc dependency completely in the future
set(SKIP_FC_TESTS ON)
add_subdirectory(fc)

add_subdirectory(range-v3)

# TODO: remove if openssl release includes keccak hash functions (currently in master branch)
include(cmake/XKCP.cmake)

set_target_properties(appbase PROPERTIES DEBUG_POSTFIX "")
set_target_properties(fc PROPERTIES DEBUG_POSTFIX "")
if(TARGET Catch2)
  set_target_properties(Catch2 PROPERTIES DEBUG_POSTFIX "")
endif()
if(TARGET Catch2WithMain)
  set_target_properties(Catch2WithMain PROPERTIES DEBUG_POSTFIX "")
endif()
