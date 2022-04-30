add_library(noir_core_v1 INTERFACE)
target_include_directories(noir_core_v1 INTERFACE ${NOIR_SOURCE_DIR} ${Boost_INCLUDE_DIR})
target_link_libraries(noir_core_v1 INTERFACE
  fmt::fmt
)

add_library(noir::core::v1 ALIAS noir_core_v1)

add_noir_test(result_test test/result_test.cpp DEPENDS noir::core::v1)
