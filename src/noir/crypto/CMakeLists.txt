add_library(noir_crypto STATIC
  blake2.cpp
  keccak.cpp
  openssl.cpp
  rand.cpp
  ripemd.cpp
  sha2.cpp
  sha3.cpp
  xxhash.cpp
)
target_include_directories(noir_crypto PRIVATE ${CMAKE_BINARY_DIR}/include)
target_link_libraries(noir_crypto
  noir_common
  b2
  XKCP
  xxhash
)
set_target_properties(noir_crypto PROPERTIES UNITY_BUILD ${NOIR_UNITY_BUILD})

add_library(noir::crypto ALIAS noir_crypto)

add_noir_test(crypto_test
  test/hash_test.cpp
  test/rand_test.cpp
DEPENDS
  noir_crypto
)
