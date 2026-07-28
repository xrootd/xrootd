# XrdClHttpObj only exists when libcurl was found
if(NOT TARGET XrdClHttpObj)
  return()
endif()

add_executable(xrdclhttp-tests-unit
        HeaderInjectionTest.cc
        )

target_link_libraries(xrdclhttp-tests-unit
        XrdClHttpObj
        GTest::gmock
        GTest::gtest
        GTest::gtest_main
        )

gtest_discover_tests(xrdclhttp-tests-unit
        PROPERTIES DISCOVERY_TIMEOUT 10)
