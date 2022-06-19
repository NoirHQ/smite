add_library(tendermint_common STATIC lib.cpp)
target_include_directories(tendermint_common PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(tendermint_common
  noir::common
  noir::thread
  spdlog::spdlog
)

add_library(tendermint::common ALIAS tendermint_common)
