add_library(noir_clist STATIC
  clist.cpp
)
target_include_directories(noir_clist PUBLIC ${PROJECT_SOURCE_DIR}/src ${Boost_INCLUDE_DIR})
target_link_libraries(noir_clist
  fmt::fmt
  noir::common
  noir::core
  eo::eo
)

add_library(noir::clist ALIAS noir_clist)

add_noir_test(clist_test test/clist_test.cpp DEPENDS noir::clist)
