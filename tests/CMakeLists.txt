if(NOT BUILD_TESTS)
  return()
endif()

include(GoogleTest)

add_subdirectory(common)

add_subdirectory(unit)
add_subdirectory(integration)

add_subdirectory(XrdPosix)

add_subdirectory( XrdSsiTests )

if(NOT ENABLE_SERVER_TESTS)
  return()
endif()

add_subdirectory(end-to-end)

add_subdirectory(krb5)
add_subdirectory(scitokens)
add_subdirectory(tls)

add_subdirectory(XrdCl)
add_subdirectory(XrdClHttpCommon)
add_subdirectory(XrdClHttp)
add_subdirectory(XrdClS3)

add_subdirectory( XrdOssTests )
add_subdirectory( XRootD )
add_subdirectory( cluster )
add_subdirectory( authenticated_cluster)
add_subdirectory( stress )
add_subdirectory( TPCTests )
add_subdirectory( badredir )
add_subdirectory( xcachewithcsi )
