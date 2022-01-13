# Don't add the duplicated cmake target "Catch2" by expected
set(EXPECTED_BUILD_TESTS OFF)
set(EXPECTED_BUILD_PACKAGE_DEB OFF)

set(SKIP_FC_TESTS ON)

add_subdirectory(appbase)
add_subdirectory(Catch2)
add_subdirectory(expected)
add_subdirectory(fc)
add_subdirectory(fmt)
add_subdirectory(tendermint)
