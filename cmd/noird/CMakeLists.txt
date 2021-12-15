add_executable(noird main.cpp)
target_include_directories(noird PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(noird noir)
