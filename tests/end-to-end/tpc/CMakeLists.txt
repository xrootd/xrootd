# The HTTP TPC tests need the XrdHttp server and the XrdClHttp client plug-in,
# which only exists when libcurl was found
if(BUILD_HTTP AND TARGET XrdClHttpObj)
  add_subdirectory(http)
endif()

add_subdirectory(root)
