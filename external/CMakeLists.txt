find_package(Git QUIET)

if( BUILD_CRC32C )

  if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
      ###########################################################
      # Update the crc32c submodule
      ###########################################################
      option(CRC32C_SUBMODULE "Check submodules during build" ON)
      if(CRC32C_SUBMODULE)
          message(STATUS "crc32c submodule update")
          execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init -- "${PROJECT_SOURCE_DIR}/external/crc32c"
                          WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                          RESULT_VARIABLE CRC32C_SUBMOD_RESULT)
          if(NOT CRC32C_SUBMOD_RESULT EQUAL "0")
              message(FATAL_ERROR "crc32c submodule update failed with ${CRC32C_SUBMOD_RESULT}, please checkout the submodules")
          endif()
      endif()
  endif()

  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/crc32c/CMakeLists.txt")
      message(FATAL_ERROR "The crc32c submodules was not downloaded! CRC32_SUBMODULE was turned off or failed. Please update submodules and try again.")
  else()
      ###########################################################
      # Configure CRC32C build
      ###########################################################
      set(CRC32C_BUILD_TESTS OFF CACHE BOOL "Disable CRC32C tests.")
      set(CRC32C_BUILD_BENCHMARKS OFF CACHE BOOL "Disable CRC32C benchmarks.")
      set(CRC32C_USE_GLOG OFF CACHE BOOL "Disable CRC32C glog.")
      set(CRC32C_INSTALL OFF CACHE BOOL "Do NOT install crc32c targets and headers.")
      ###########################################################
      # Set up utility variables for convenience
      ###########################################################
      set( HAVE_CRC32C         TRUE                                             PARENT_SCOPE )
      set( CRC32C_INCLUDE_DIR  "${PROJECT_SOURCE_DIR}/external/crc32c/include/" PARENT_SCOPE )
      set( CRC32C_LIBRARY_DIR  "${PROJECT_BINARY_DIR}external/crc32c/"          PARENT_SCOPE )
      set( CRC32C_STATIC_LIB   "crc32c"                                         PARENT_SCOPE )
      ###########################################################
      # Make the crc32c library reallocable so it can be used when
      # making a shared object
      ###########################################################
      set( CMAKE_POSITION_INDEPENDENT_CODE ON )
      add_subdirectory(crc32c)
  endif()

endif()
