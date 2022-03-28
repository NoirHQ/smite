add_executable(smite
  app.cpp
  main.cpp
  commands/lib.cpp
)
target_include_directories(smite PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(smite
  tendermint::abci
  tendermint::node
)
set_target_properties(smite PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})
