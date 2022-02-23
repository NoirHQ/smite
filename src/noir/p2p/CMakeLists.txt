target_sources(noir
  PRIVATE
    p2p.cpp)

add_noir_test(p2p_test p2p_test.cpp)
