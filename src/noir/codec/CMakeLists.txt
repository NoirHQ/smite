add_executable(scale_test scale_test.cpp)
target_link_libraries(scale_test noir Catch2WithMain)

add_executable(rlp_test rlp_test.cpp)
target_link_libraries(rlp_test noir Catch2WithMain)
