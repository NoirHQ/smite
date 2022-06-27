add_library(noir_core STATIC
  basic_errors.cpp
)
target_include_directories(noir_core PUBLIC ${PROJECT_SOURCE_DIR}/src ${Boost_INCLUDE_DIR})
target_link_libraries(noir_core
  fmt::fmt
)
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
  set(THREADS_PREFER_PTHREAD_FLAG TRUE)
  find_package(Threads REQUIRED)
  target_link_libraries(noir_core Threads::Threads)
endif()

set_target_properties(noir_core PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})

add_library(noir::core ALIAS noir_core)

add_noir_test(error_test test/error_test.cpp DEPENDS noir::core)
add_noir_test(result_test test/result_test.cpp DEPENDS noir::core)
