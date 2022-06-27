function(add_noir_test TARGET)
  if(NOIR_BUILD_TESTS)
    cmake_parse_arguments(NOIR_TEST "" "" "DEPENDS" ${ARGN})
    if("${NOIR_TEST_DEPENDS}" STREQUAL "")
      set(NOIR_TEST_DEPENDS noir)
    endif()
    add_executable(${TARGET} ${NOIR_TEST_UNPARSED_ARGUMENTS})
    target_link_libraries(${TARGET} ${NOIR_TEST_DEPENDS} Catch2::Catch2WithMain)
    catch_discover_tests(${TARGET})
  endif()
endfunction()

add_library(noir STATIC)
target_link_libraries(noir
  noir::common
  noir::consensus
  noir::core
  noir::crypto
  noir::jmt
  noir::p2p
  noir::rpc
  noir::tx_pool
  softfloat
)

add_subdirectory(eo)
add_subdirectory(noir)
add_subdirectory(tendermint)
