add_executable(xrdoucutils-unit-tests
  XrdOucBearerTokenTests.cc
  XrdOucUtilsTests.cc
)

target_link_libraries(xrdoucutils-unit-tests XrdUtils GTest::gtest GTest::gtest_main)

gtest_discover_tests(xrdoucutils-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)

find_package(CURL QUIET)
if(CURL_FOUND AND TARGET XrdClHttpObj)
  add_executable(xrdclhttp-bearer-token-tests
    XrdClHttpGetBearerTokenTests.cc
  )

  target_link_libraries(xrdclhttp-bearer-token-tests
    XrdClHttpObj
    XrdCl
    GTest::gtest
    GTest::gtest_main
  )

  target_include_directories(xrdclhttp-bearer-token-tests PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
  )

  gtest_discover_tests(xrdclhttp-bearer-token-tests
    PROPERTIES DISCOVERY_TIMEOUT 10)
endif()
