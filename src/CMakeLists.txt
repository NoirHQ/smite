function(add_noir_test TARGET)
  if(BUILD_TESTING)
    add_executable(${TARGET} ${ARGN})
    target_link_libraries(${TARGET} noir Catch2WithMain)
    catch_discover_tests(${TARGET})
  endif()
endfunction()

set(NOIR_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(noir STATIC)
target_include_directories(noir
  PUBLIC
    ${NOIR_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../proto
)
target_link_libraries(noir
  appbase
  b2
  expected
  fc
  fixed_string
  fmt
  range-v3
  rocksdb
  scope-lite
  softfloat
  websocketpp
  XKCP
  xxhash
)
if(APPLE)
  target_link_libraries(noir "-framework CoreFoundation" "-framework Security")
endif()

add_subdirectory(eth)
add_subdirectory(noir)
