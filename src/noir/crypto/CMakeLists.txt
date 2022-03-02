target_sources(noir
  PRIVATE
    blake2.cpp
    keccak.cpp
    openssl.cpp
    rand.cpp
    ripemd.cpp
    sha2.cpp
    sha3.cpp
    xxhash.cpp)

add_noir_test(crypto_test
  test/hash_test.cpp
  test/rand_test.cpp
)
