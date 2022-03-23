function(add_noir_test TARGET)
  if(BUILD_TESTING)
    cmake_parse_arguments(NOIR_TEST "" "" "DEPENDS" ${ARGN})
    if("${NOIR_TEST_DEPENDS}" STREQUAL "")
      set(NOIR_TEST_DEPENDS noir)
    endif()
    add_executable(${TARGET} ${NOIR_TEST_UNPARSED_ARGUMENTS})
    target_link_libraries(${TARGET} ${NOIR_TEST_DEPENDS} Catch2WithMain)
    catch_discover_tests(${TARGET})
  endif()
endfunction()

set(NOIR_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# add noir_common target first for common dependencies
add_subdirectory(noir/common)

add_library(noir STATIC)
target_link_libraries(noir
  noir::common
  noir::consensus
  noir::crypto
  noir::jmt
  noir::p2p
  noir::proto
  noir::rpc
  noir::tx_pool
  softfloat
)

add_subdirectory(eth)
add_subdirectory(noir)
