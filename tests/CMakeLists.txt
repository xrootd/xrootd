if(NOT BUILD_TESTS)
  return()
endif()

include(GoogleTest)

add_subdirectory(common)

add_subdirectory(unit)
add_subdirectory(integration)

add_subdirectory( XrdSsiTests )

if(NOT ENABLE_SERVER_TESTS)
  return()
endif()

add_subdirectory(end-to-end)

add_subdirectory(XrdClHttpCommon)
add_subdirectory(XrdClS3)

add_subdirectory( XrdOssTests )
