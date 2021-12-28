add_library(noir STATIC)
target_include_directories(noir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(noir
  appbase
  expected
  fc
  tendermint
)
if(APPLE)
  target_link_libraries(noir "-framework CoreFoundation" "-framework Security")
endif()

add_subdirectory(noir)
