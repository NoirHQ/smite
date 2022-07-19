add_executable(noird
  main.cpp
  noir/commands/cli.cpp
  noir/commands/init.cpp
  noir/commands/reset.cpp
  noir/commands/root.cpp
  noir/commands/version.cpp
)
target_include_directories(noird PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(noird noir tendermint::log)
