file(GLOB HEADERS "*.h")
add_library(net_plugin
        net_plugin.cpp
        ${HEADERS})

target_link_libraries(net_plugin appbase fc)
target_include_directories(net_plugin PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../)

add_executable(net_test main.cpp)
target_link_libraries(net_test net_plugin)
