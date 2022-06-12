add_library(noir_p2p STATIC
  conn/merlin.cpp
  conn/secret_connection.cpp
  p2p.cpp
)
target_link_libraries(noir_p2p
  RocksDB::rocksdb
  noir::codec
  noir::common
  noir::crypto
  noir::proto_legacy
  sodium
)
set_target_properties(noir_p2p PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})

add_library(noir::p2p ALIAS noir_p2p)

add_noir_test(connection_test conn/test/connection_test.cpp DEPENDS noir_p2p)
add_noir_test(p2p_test test/p2p_test.cpp DEPENDS noir_p2p)
