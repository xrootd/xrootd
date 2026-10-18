if(NOT BUILD_HTTP)
  return()
endif()

# Test-only plugins of plugin.bats. The test needs XrdHttp and its TPC
# handler, which the first tests use.

# XrdXrootdRedirPI plugin: rewrites the redirect target host/port (host:port
# form via Redirect()) and the full URL (URL form via RedirectURL()) to two
# distinct sentinels the test script greps for.
add_library(XrdRedirTestPlugin MODULE redir_test_plugin.cc)

target_link_libraries(XrdRedirTestPlugin PRIVATE XrdUtils)

set_target_properties(XrdRedirTestPlugin
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "libXrdRedirTestPlugin"
        SUFFIX ".so")

# XrdOssWrapper plugin: forces XrdOfs::open to emit a URL-form SFS_REDIRECT
# (port=-1 + FileURL) for any path under /urlredir/, so the URL-form branch
# of fsRedirPI() can be exercised by the system test.
add_library(XrdOssUrlRedirTestPlugin MODULE oss_url_redir_plugin.cc)

target_link_libraries(XrdOssUrlRedirTestPlugin PRIVATE XrdServer XrdUtils)

set_target_properties(XrdOssUrlRedirTestPlugin
    PROPERTIES
        PREFIX ""
        OUTPUT_NAME "libXrdOssUrlRedirTestPlugin"
        SUFFIX ".so")

add_bats_test(plugin.bats)
