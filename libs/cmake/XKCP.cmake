set(XKCP_ARCH ${CMAKE_CURRENT_SOURCE_DIR}/XKCP/bin/generic64)

# suppress non-existent include error
file(MAKE_DIRECTORY ${XKCP_ARCH}/libXKCP.a.headers)

# workaround fix to github linux build workflow
if(NOT APPLE)
  add_custom_command(
    OUTPUT ${XKCP_ARCH}/libXKCP.a
    COMMAND CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} make generic64/libXKCP.a
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/XKCP)
else()
  add_custom_command(
    OUTPUT ${XKCP_ARCH}/libXKCP.a
    COMMAND make generic64/libXKCP.a
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/XKCP)
endif()

add_cached_library(XKCP ${XKCP_ARCH}/libXKCP.a
  INTERFACE
    ${XKCP_ARCH}/libXKCP.a.headers)
add_dependencies(XKCP ${XKCP_ARCH}/libXKCP.a)
