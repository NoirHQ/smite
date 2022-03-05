function(add_subdirectory_if DIR)
  if(${ARGN})
    add_subdirectory(${DIR})
  endif()
endfunction()

function(add_cached_library TARGET LIBRARY)
  if(EXISTS ${LIBRARY})
    add_library(${TARGET} STATIC IMPORTED GLOBAL)
    set_target_properties(${TARGET} PROPERTIES IMPORTED_LOCATION ${LIBRARY})
    if(${ARGC} GREATER_EQUAL 3)
      target_include_directories(${TARGET} ${ARGN})
    endif()
  endif()
endfunction()

if(NOT USE_CACHED_LIBS)
  return()
endif()

add_cached_library(softfloat ${CMAKE_CURRENT_BINARY_DIR}/softfloat/libsoftfloat.a
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/softfloat/source/include
    ${CMAKE_CURRENT_SOURCE_DIR}/softfloat/source/8086-SSE
    ${CMAKE_CURRENT_SOURCE_DIR}/softfloat/build/Linux-x86_64-GCC)

add_cached_library(appbase ${CMAKE_CURRENT_BINARY_DIR}/appbase/libappbase.a
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/appbase/include
    ${Boost_INCLUDE_DIR})

add_cached_library(Catch2 ${CMAKE_CURRENT_BINARY_DIR}/Catch2/src/libCatch2.a
  INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Catch2/src)

add_cached_library(Catch2WithMain ${CMAKE_CURRENT_BINARY_DIR}/Catch2/src/libCatch2Main.a)
if(TARGET Catch2WithMain AND TARGET Catch2)
  target_link_libraries(Catch2WithMain INTERFACE Catch2)
endif()
