add_library(tendermint_core INTERFACE)
target_link_libraries(tendermint_core INTERFACE noir::core::v1)

add_library(tendermint::core ALIAS tendermint_core)
