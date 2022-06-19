add_library(noir_codec STATIC
  errors.cpp
)
target_include_directories(noir_codec PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(noir_codec
  noir::common
  protobuf::libprotobuf
)

add_library(noir::codec ALIAS noir_codec)

#add_noir_test(proto3_test test/proto3_test.cpp DEPENDS noir_common)
#add_noir_test(rlp_test test/rlp_test.cpp DEPENDS noir_common)
#add_noir_test(scale_test test/scale_test.cpp DEPENDS noir_codec)
add_noir_test(bcs_test test/bcs_test.cpp DEPENDS noir::codec)
