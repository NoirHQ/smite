add_library(noir_core_v1 STATIC
  basic_errors.cpp
)
target_include_directories(noir_core_v1 PUBLIC ${NOIR_SOURCE_DIR} ${Boost_INCLUDE_DIR})
target_link_libraries(noir_core_v1
  fmt::fmt
)
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads REQUIRED)
  target_link_libraries(noir_core_v1 PRIVATE Threads::Threads)
endif()

add_library(noir::core::v1 ALIAS noir_core_v1)

add_noir_test(error_test test/error_test.cpp DEPENDS noir::core::v1)
add_noir_test(result_test test/result_test.cpp DEPENDS noir::core::v1)

add_noir_example(channel_example example/channel_example.cpp DEPENDS noir::core::v1)
