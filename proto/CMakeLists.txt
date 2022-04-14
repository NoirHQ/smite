list(APPEND PROTO_SOURCES
  gogoproto/gogo.proto
  tendermint/statesync/types.proto
  tendermint/consensus/types.proto
  tendermint/consensus/wal.proto
  tendermint/crypto/proof.proto
  tendermint/crypto/keys.proto
  tendermint/abci/types.proto
  tendermint/types/evidence.proto
  tendermint/types/events.proto
  tendermint/types/canonical.proto
  tendermint/types/types.proto
  tendermint/types/validator.proto
  tendermint/types/block.proto
  tendermint/types/params.proto
  tendermint/privval/types.proto
  tendermint/state/types.proto
  tendermint/libs/bits/types.proto
  tendermint/blockchain/types.proto
  tendermint/version/types.proto
  tendermint/p2p/types.proto
  tendermint/p2p/conn.proto
  tendermint/p2p/pex.proto
  tendermint/mempool/types.proto
  tendermint/rpc/grpc/types.proto
  tendermint/store/types.proto
)

set(Protobuf_USE_STATIC_LIBS ON)
find_package(Protobuf REQUIRED)

set(PROTO_PATH ${PROJECT_SOURCE_DIR}/proto)
foreach(PROTO ${PROTO_SOURCES})
  set(PROTO_OUTPUT ${PROTO})
  string(REPLACE "\.proto" ".pb.cc" PROTO_OUTPUT ${PROTO_OUTPUT})
  if(NOT EXISTS ${PROTO_OUTPUT})
    execute_process(
      COMMAND protoc --proto_path=${PROTO_PATH} --cpp_out=${PROTO_PATH} ${PROTO_PATH}/${PROTO}
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    list(APPEND PROTO_GENERATED ${PROTO_OUTPUT})
  endif()
endforeach()

add_library(tendermint_proto STATIC ${PROTO_GENERATED})
target_include_directories(tendermint_proto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tendermint_proto protobuf::libprotobuf)

add_library(tendermint::proto ALIAS tendermint_proto)
