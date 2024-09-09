if(NOT BUILD_TESTS)
  return()
endif()

include(GoogleTest)

add_subdirectory(common)

add_subdirectory(XrdCl)
add_subdirectory(XrdCeph)
add_subdirectory(XrdEc)

add_subdirectory(XrdHttpTests)

add_subdirectory(XrdOucTests)

add_subdirectory( XrdSsiTests )

add_subdirectory(XrdTpcTests)

if(NOT ENABLE_SERVER_TESTS)
  return()
endif()

add_subdirectory( XRootD )
add_subdirectory( cluster )
