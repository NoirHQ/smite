add_library(noir_p2p STATIC
  p2p.cpp
)
target_link_libraries(noir_p2p
  noir_common
  noir_crypto
  rocksdb
)
set_target_properties(noir_p2p PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})

add_library(noir::p2p ALIAS noir_p2p)

add_noir_test(connection_test conn/test/connection_test.cpp DEPENDS noir_p2p Sodium)
add_noir_test(p2p_test test/p2p_test.cpp DEPENDS noir_p2p)
