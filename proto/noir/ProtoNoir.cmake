list(APPEND LEGACY_PROTO_SOURCES
  gogoproto/gogo.proto
  noir/abci/types.proto
  noir/blocksync/message.proto
  noir/consensus/types.proto
  noir/consensus/wal.proto
  noir/crypto/keys.proto
  noir/crypto/proof.proto
  noir/libs/bits/types.proto
  noir/mempool/types.proto
  noir/p2p/conn.proto
  noir/p2p/pex.proto
  noir/p2p/types.proto
  noir/privval/service.proto
  noir/privval/types.proto
  noir/rpc/grpc/types.proto
  noir/state/types.proto
  noir/statesync/types.proto
  noir/types/block.proto
  noir/types/canonical.proto
  noir/types/events.proto
  noir/types/evidence.proto
  noir/types/params.proto
  noir/types/types.proto
  noir/types/validator.proto
  noir/version/types.proto
)

find_package(Protobuf REQUIRED)

set(PROTO_PATH ${PROJECT_SOURCE_DIR}/proto)
foreach(PROTO ${LEGACY_PROTO_SOURCES})
  set(PROTO_OUTPUT ${PROTO})
  string(REPLACE "\.proto" ".pb.cc" PROTO_OUTPUT ${PROTO_OUTPUT})
  if(NOT EXISTS ${PROTO_OUTPUT})
    execute_process(
      COMMAND protoc --proto_path=${PROTO_PATH} --cpp_out=${PROTO_PATH} ${PROTO_PATH}/${PROTO}
      WORKING_DIRECTORY ${PROJECT_SOURCE_DIR})
    list(APPEND LEGACY_PROTO_GENERATED ${PROTO_OUTPUT})
  endif()
endforeach()

#add_library(noir_proto STATIC ${LEGACY_PROTO_GENERATED})
#target_include_directories(noir_proto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
#target_link_libraries(noir_proto protobuf::libprotobuf)
#
#add_library(noir::proto ALIAS noir_proto)
