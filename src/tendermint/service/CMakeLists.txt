add_library(tendermint_service STATIC
  service.cpp
)
target_include_directories(tendermint_service PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(tendermint_service
  noir::common
  noir::log
)

add_library(tendermint::service ALIAS tendermint_service)
