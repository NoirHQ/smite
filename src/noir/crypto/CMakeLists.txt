target_sources(noir
  PRIVATE
    keccak.cpp)

add_noir_test(crypto_test crypto_test.cpp)