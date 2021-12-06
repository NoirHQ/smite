add_subdirectory(appbase)
add_subdirectory(fc)
add_subdirectory(tendermint)

add_library(quadrable INTERFACE)
target_include_directories(quadrable
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/quadrable/include
    ${CMAKE_CURRENT_SOURCE_DIR}/quadrable/external
    ${CMAKE_CURRENT_SOURCE_DIR}/quadrable/external/hoytech-cpp
)
