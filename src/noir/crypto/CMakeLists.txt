target_sources(noir
  PRIVATE
    blake2.cpp
    keccak.cpp
    openssl.cpp
    ripemd.cpp
    sha2.cpp
    sha3.cpp)

add_noir_test(crypto_test crypto_test.cpp)
