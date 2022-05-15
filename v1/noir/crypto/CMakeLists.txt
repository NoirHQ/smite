add_library(noir_crypto_v1 STATIC
  hash/blake2.cpp
  hash/keccak.cpp
  hash/ripemd.cpp
  hash/sha2.cpp
  hash/sha3.cpp
  hash/xxhash.cpp
  openssl/message_digest.cpp
  rand/rand.cpp
)
target_include_directories(noir_crypto_v1 PUBLIC ${CMAKE_BINARY_DIR}/include)
target_link_libraries(noir_crypto_v1
  noir::common::v1
  libb2::libb2
  OpenSSL::Crypto
  XKCP
  xxHash::xxhash
)
set_target_properties(noir_crypto_v1 PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})

add_library(noir::crypto::v1 ALIAS noir_crypto_v1)

add_noir_test(hash_test test/hash_test.cpp DEPENDS noir::crypto::v1)
add_noir_test(rand_test test/rand_test.cpp DEPENDS noir::crypto::v1)
