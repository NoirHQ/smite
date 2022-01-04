if(BUILD_TESTING)
  target_sources(noir_tests
    PRIVATE
      scale_test.cpp
      rlp_test.cpp
  )
endif()

