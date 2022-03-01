find_library(B2_LIBRARY libb2.a)
if(B2_LIBRARY)
  find_path(B2_INCLUDE_DIR blake2.h)
  add_cached_library(b2 ${B2_LIBRARY})
  # B2_INCLUDE_DIR is /usr/local/include for libb2 installed by brew,
  # and including it causes system include conflicts.
  #target_include_directories(b2 INTERFACE ${B2_INCLUDE_DIR})
  file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${B2_INCLUDE_DIR}/blake2.h ${CMAKE_BINARY_DIR}/include/blake2.h)
endif()
