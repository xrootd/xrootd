find_package(CURL REQUIRED)

add_executable(xrdclhttp-tests-unit
        XrdClHttpFilesystemTests.cc
        )

# Both test sources include XrdClHttpFilesystem.cc, hence they cannot be part of
# the same executable, otherwise its definitions would be duplicated.
add_executable(xrdclhttp-parsetokenfile-tests-unit
        ParseTokenFileTests.cc
        )

add_library(XrdClHttpWithoutFilesystem OBJECT
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpFactory.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpFile.cc
#   ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpFilesystem.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOpChecksum.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOpCopy.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOpDelete.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOpListdir.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOpMkcol.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOpOpen.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOpOptions.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOpPut.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOpQuery.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOpRead.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOpReadV.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOps.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOpStat.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOpTape.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOptionsCache.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpParseTimeout.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpTape.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpUtil.cc
)

target_link_libraries(XrdClHttpWithoutFilesystem
  PRIVATE
    XrdCl
    XrdUtils
    XrdXml
    CURL::libcurl
    OpenSSL::Crypto
)

foreach(target xrdclhttp-tests-unit xrdclhttp-parsetokenfile-tests-unit)
  target_link_libraries(${target}
      GTest::gmock
      GTest::gtest
      GTest::gtest_main
      XrdClHttpWithoutFilesystem)

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
    target_link_libraries(${target} stdc++fs)
  endif()

  gtest_discover_tests(${target}
    PROPERTIES DISCOVERY_TIMEOUT 10)
endforeach()
