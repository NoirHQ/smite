add_library(noir_crypto STATIC
  hash/blake2.cpp
  hash/keccak.cpp
  hash/ripemd.cpp
  hash/sha2.cpp
  hash/sha3.cpp
  hash/xxhash.cpp
  openssl/hash.cpp
  rand/rand.cpp
)
target_include_directories(noir_crypto PUBLIC ${CMAKE_BINARY_DIR}/include)
target_link_libraries(noir_crypto
  noir_common
  libb2::libb2
  XKCP
  xxHash::xxhash
)
set_target_properties(noir_crypto PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})

add_library(noir::crypto ALIAS noir_crypto)

add_noir_test(crypto_test
  test/hash_test.cpp
  test/rand_test.cpp
DEPENDS
  noir_crypto
)
