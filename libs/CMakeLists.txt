set(CMAKE_BUILD_TYPE Release)

include(cmake/CachedLibs.cmake)

# add subdirectories if cached libs not found
add_subdirectory_if(softfloat NOT TARGET softfloat)
add_subdirectory_if(appbase NOT TARGET appbase)

# websocketpp in conan requires the specific version of boost and other dependencies.
# Let's include as header-only library here.
add_definitions(-DWEBSOCKETPP_STRICT_MASKING -DBOOST_ASIO_HAS_STD_CHRONO)
add_library(websocketpp INTERFACE)
target_include_directories(websocketpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/websocketpp)

# TODO: remove fc dependency completely in the future
set(SKIP_FC_TESTS ON)
add_subdirectory(fc)

add_subdirectory(range-v3)

# TODO: remove if openssl release includes keccak hash functions (currently in master branch)
include(cmake/XKCP.cmake)

set(sodium_USE_STATIC_LIBS ON)
include(cmake/Findsodium.cmake)
set_target_properties(sodium PROPERTIES IMPORTED_GLOBAL TRUE)

set_target_properties(appbase PROPERTIES DEBUG_POSTFIX "")
set_target_properties(fc PROPERTIES DEBUG_POSTFIX "")
