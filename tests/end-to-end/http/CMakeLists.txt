add_subdirectory(header)

if(NOT BUILD_HTTP)
  return()
endif()

add_subdirectory(put)

add_bats_test(cors.bats)
add_bats_test(digest.bats)
add_bats_test(headers.bats)
add_bats_test(html.bats)
add_bats_test(monitoring.bats)
add_bats_test(port.bats)
add_bats_test(redirect.bats)
add_bats_test(range.bats)
add_bats_test(smuggling.bats)
add_bats_test(trailers.bats)

# The tests of the client use the XrdClHttp plugin, which only exists when
# libcurl was found
if(TARGET XrdClHttpObj)
  add_bats_test(dav.bats)
  add_bats_test(errors.bats)

  find_package(CURL REQUIRED)
  add_executable(xrdhttp-stress HttpStress.cc)
  target_link_libraries(xrdhttp-stress GTest::gtest GTest::gtest_main CURL::libcurl)
  add_bats_test(stress.bats)
endif()

if(BUILD_HTTP AND TARGET XrdClHttpObj AND TARGET xrootd AND TARGET xrdfs)
  add_subdirectory(worker)
endif()
