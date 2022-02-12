target_sources(noir
  PRIVATE
    blake2.cpp
    keccak.cpp
    sha2.cpp)

add_noir_test(crypto_test crypto_test.cpp)
