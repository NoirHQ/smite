add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tendermint
  COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/tendermint)
add_custom_target(tendermint_dir DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tendermint)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/tendermint/tendermint.a
  COMMAND go build -buildmode=c-archive -o ${CMAKE_CURRENT_BINARY_DIR}/tendermint
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_custom_target(tendermint_go DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/tendermint/tendermint.a)
add_dependencies(tendermint_go tendermint_dir)

add_library(tendermint STATIC IMPORTED GLOBAL)
set_target_properties(tendermint PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/tendermint/tendermint.a)
target_include_directories(tendermint
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(tendermint tendermint_go)
