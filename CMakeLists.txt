cmake_minimum_required(VERSION 3.16)

project(noir)

set(CMAKE_CXX_STANDARD 20)

if(APPLE)
  set(BOOST_ROOT "/usr/local/opt/boost")
  set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
endif()

add_subdirectory(libs)
add_subdirectory(src)
add_subdirectory(cmd)
