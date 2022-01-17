function(add_noir_test TARGET)
  if(BUILD_TESTING)
    add_executable(${TARGET} ${ARGN})
    target_link_libraries(${TARGET} noir Catch2WithMain)
    catch_discover_tests(${TARGET})
  endif()
endfunction()

add_library(noir STATIC)
target_include_directories(noir PUBLIC ${NOIR_SOURCE_DIR})
target_link_libraries(noir
  appbase
  expected
  fc
  tendermint
  websocketpp
)
if(APPLE)
  target_link_libraries(noir "-framework CoreFoundation" "-framework Security")
endif()

add_subdirectory(codec)
add_subdirectory(common)
add_subdirectory(consensus)
add_subdirectory(p2p)
add_subdirectory(tendermint)
add_subdirectory(rpc)
