# Don't add the duplicated cmake target "Catch2" by expected
set(EXPECTED_BUILD_TESTS OFF)
set(EXPECTED_BUILD_PACKAGE_DEB OFF)

set(SKIP_FC_TESTS ON)

add_definitions(-DWEBSOCKETPP_STRICT_MASKING -DBOOST_ASIO_HAS_STD_CHRONO)

add_library(websocketpp INTERFACE)
target_include_directories(websocketpp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/websocketpp)

add_subdirectory(appbase)
add_subdirectory(Catch2)
add_subdirectory(expected)
add_subdirectory(fc)
add_subdirectory(tendermint)
add_subdirectory(websocketpp)
