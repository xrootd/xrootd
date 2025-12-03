if(NOT BUILD_TESTS)
  return()
endif()

include(GoogleTest)

add_subdirectory(common)

add_subdirectory(XrdCl)
add_subdirectory(XrdCeph)
add_subdirectory(XrdEc)
add_subdirectory(XrdPosix)

add_subdirectory(XrdHttpTests)

add_subdirectory(XrdOucTests)

add_subdirectory( XrdSsiTests )

add_subdirectory(XrdHttpTpc)

add_subdirectory(XrdPfcTests)

if(NOT ENABLE_SERVER_TESTS)
  return()
endif()

add_subdirectory(krb5)
add_subdirectory(scitokens)
add_subdirectory(tls)

add_subdirectory(XrdClCurlCommon)
add_subdirectory(XrdClCurl)
add_subdirectory(XrdClS3)

add_subdirectory( XrdOssTests )
add_subdirectory( XRootD )
add_subdirectory( cluster )
add_subdirectory( authenticated_cluster)
add_subdirectory( stress )
add_subdirectory( TPCTests )
add_subdirectory( badredir )
add_subdirectory( xcachewithcsi )
