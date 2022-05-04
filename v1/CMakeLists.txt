include(NoirTest)
set(NOIR_TARGET_DEFAULT_DEPENDS noir_v1)

set(NOIR_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(noir_v1 INTERFACE)
target_link_libraries(noir_v1
  INTERFACE
)

add_subdirectory(noir)
add_subdirectory(tendermint)