add_executable(smite
  main.cpp
  noir/commands/command.cpp
  noir/commands/consensus_test.cpp
  noir/commands/debug.cpp
  noir/commands/init.cpp
  noir/commands/start.cpp
  noir/commands/unsafe_reset_all.cpp
  noir/commands/version.cpp
)
target_include_directories(smite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(smite noir)
set_target_properties(smite PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})
