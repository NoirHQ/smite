add_library(noir STATIC)
target_include_directories(noir PUBLIC ${NOIR_SOURCE_DIR})
target_link_libraries(noir
  appbase
  expected
  fc
  tendermint
)
if(APPLE)
  target_link_libraries(noir "-framework CoreFoundation" "-framework Security")
endif()

if(BUILD_TESTING)
  add_executable(noir_tests)
  target_link_libraries(noir_tests noir Catch2WithMain)
endif()

add_subdirectory(codec)
add_subdirectory(common)
add_subdirectory(tendermint)

