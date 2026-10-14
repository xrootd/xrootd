# XrdClHttpObj only exists when libcurl was found
if(NOT TARGET XrdClHttpObj)
  return()
endif()

add_executable(xrdclhttp-tests-unit
        HeaderBuilderAppendMissingTest.cc
        HeaderBuilderEntrySeparationTest.cc
        HeaderBuilderForbiddenEntryTest.cc
        HeaderBuilderForbiddenHeaderTest.cc
        HeaderBuilderParsingTest.cc
        ParseTokenFileTests.cc
        )

target_include_directories(xrdclhttp-tests-unit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(xrdclhttp-tests-unit
        XrdClHttpObj
        GTest::gmock
        GTest::gtest
        GTest::gtest_main
        )

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
  AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
  target_link_libraries(xrdclhttp-tests-unit stdc++fs)
endif()

gtest_discover_tests(xrdclhttp-tests-unit
        PROPERTIES DISCOVERY_TIMEOUT 10)
find_package(CURL REQUIRED)

add_executable(xrdclhttp-tpc-tests-unit
        XrdClHttpFilesystemTests.cc
        )

file(GLOB_RECURSE XrdClHttpWithoutFilesystemSources
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/*.cc
)
list(REMOVE_ITEM XrdClHttpWithoutFilesystemSources
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpFilesystem.cc
)

add_library(XrdClHttpWithoutFilesystem OBJECT
  ${XrdClHttpWithoutFilesystemSources}
)

target_link_libraries(XrdClHttpWithoutFilesystem
  PRIVATE
    XrdCl
    XrdUtils
    XrdXml
    CURL::libcurl
    OpenSSL::Crypto
)

target_link_libraries(xrdclhttp-tpc-tests-unit
    GTest::gmock
    GTest::gtest
    GTest::gtest_main
    XrdClHttpWithoutFilesystem)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
  AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
  target_link_libraries(xrdclhttp-tpc-tests-unit stdc++fs)
endif()

gtest_discover_tests(xrdclhttp-tpc-tests-unit
  PROPERTIES DISCOVERY_TIMEOUT 10)
