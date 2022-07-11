add_library(noir_app STATIC
  socket_app.cpp
)
target_include_directories(noir_app PUBLIC ${PROJECT_SOURCE_DIR}/src ${Boost_INCLUDE_DIR})
target_link_libraries(noir_app
  noir::codec
  noir::common
  noir::core
  noir::crypto
  tendermint::abci
)
set_target_properties(noir_app PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})

add_library(noir::app ALIAS noir_app)
