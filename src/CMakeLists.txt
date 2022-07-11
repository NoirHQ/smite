function(add_noir_example TARGET)
  if(NOIR_BUILD_TESTS)
    cmake_parse_arguments(NOIR_TEST "" "" "DEPENDS" ${ARGN})
    if("${NOIR_TEST_DEPENDS}" STREQUAL "")
      set(NOIR_TEST_DEPENDS noir)
    endif()
    add_executable(${TARGET} ${NOIR_TEST_UNPARSED_ARGUMENTS})
    target_link_libraries(${TARGET} ${NOIR_TEST_DEPENDS})
  endif()
endfunction()

function(add_noir_benchmark TARGET)
  add_noir_example(${TARGET} ${ARGN})
  if(NOIR_BUILD_TESTS)
    target_link_libraries(${TARGET} Catch2::Catch2WithMain)
  endif()
endfunction()

function(add_noir_test TARGET)
  add_noir_benchmark(${TARGET} ${ARGN})
  if(NOIR_BUILD_TESTS)
    catch_discover_tests(${TARGET})
  endif()
endfunction()

add_library(noir INTERFACE)
target_link_libraries(noir INTERFACE
  noir::common
  noir::consensus
  noir::core
  noir::crypto
  #noir::jmt
  noir::p2p
  #noir::rpc
  softfloat
)

add_subdirectory(eo)
add_subdirectory(noir)
add_subdirectory(tendermint)
