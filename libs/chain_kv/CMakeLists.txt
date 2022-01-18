add_library(chain_kv INTERFACE)
target_include_directories(chain_kv INTERFACE include)
target_link_libraries(chain_kv INTERFACE fc fmt rocksdb softfloat)
add_subdirectory(unit_tests)

