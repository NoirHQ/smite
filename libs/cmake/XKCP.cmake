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

add_custom_target(XKCP_build DEPENDS ${XKCP_ARCH}/libXKCP.a)

add_library(XKCP STATIC IMPORTED GLOBAL)
add_dependencies(XKCP XKCP_build)
set_target_properties(XKCP PROPERTIES IMPORTED_LOCATION ${XKCP_ARCH}/libXKCP.a)
target_include_directories(XKCP INTERFACE ${XKCP_ARCH}/libXKCP.a.headers)
