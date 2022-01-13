add_executable(validator_test validator_test.cpp)
target_link_libraries(validator_test noir Catch2WithMain)

add_executable(consensus_state_test consensus_state_test.cpp)
target_link_libraries(consensus_state_test noir Catch2WithMain)

add_executable(db_test db/db_test.cpp)
target_link_libraries(db_test noir Catch2WithMain)

add_executable(store_test store/store_test.cpp)
target_link_libraries(store_test noir Catch2WithMain)
