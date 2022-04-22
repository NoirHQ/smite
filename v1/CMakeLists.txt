include(NoirTest)
set(NOIR_TEST_DEFAULT_DEPENDS noir_v1)

add_library(noir_v1 INTERFACE)
target_link_libraries(noir_v1
  INTERFACE
)

add_subdirectory(noir)
