add_library(tendermint_node STATIC
  node.cpp
  plugin.cpp
)
target_include_directories(tendermint_node PUBLIC ${NOIR_SOURCE_DIR})
target_link_libraries(tendermint_node
  tendermint::abci
)

add_library(tendermint::node ALIAS tendermint_node)
