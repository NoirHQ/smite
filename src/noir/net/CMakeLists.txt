file(GLOB HEADERS "*.h")
file(GLOB CONSENSUS_HEADERS "consensus/*.h")
add_library(net_plugin
        net_plugin.cpp
        ${CONSENSUS_HEADERS}
        ${HEADERS})

target_link_libraries(net_plugin appbase fc expected)
target_include_directories(net_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../)

add_executable(net_test main.cpp)
target_link_libraries(net_test net_plugin)

add_executable(validator_test consensus/validator_test.cpp)
target_link_libraries(validator_test net_plugin Catch2WithMain noir)
