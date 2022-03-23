add_executable(noird
  main.cpp
  noir/commands/command.cpp
  noir/commands/consensus_test.cpp
  noir/commands/debug.cpp
  noir/commands/init.cpp
  noir/commands/node_test.cpp
  noir/commands/start.cpp
  noir/commands/version.cpp
)
target_include_directories(noird PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(noird noir)
set_target_properties(noird PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})
