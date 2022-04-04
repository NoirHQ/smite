add_library(tendermint_net STATIC
  conn.cpp
)
target_include_directories(tendermint_net PUBLIC ${NOIR_SOURCE_DIR})
target_link_libraries(tendermint_net
  tendermint::common
  tendermint::log
)

add_library(tendermint::net ALIAS tendermint_net)
