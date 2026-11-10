# XrdClHttpObj only exists when libcurl was found
if(NOT TARGET XrdClHttpObj)
  return()
endif()

find_package(CURL REQUIRED)

add_unit_tests(
  SOURCES
    XrdClHttpHeaderBuilderAppendMissing.cc
    XrdClHttpHeaderBuilderEntrySeparation.cc
    XrdClHttpHeaderBuilderForbiddenEntry.cc
    XrdClHttpHeaderBuilderForbiddenHeader.cc
    XrdClHttpHeaderBuilderParsing.cc
    XrdClHttpParseTimeout.cc
    XrdClHttpTape.cc
    XrdClHttpTokenFileParser.cc
    XrdClHttpUtil.cc
  LIBRARIES
    XrdClHttpObj
    XrdCl
    CURL::libcurl
  INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src
)

# The tests of the third-party copy compile XrdClHttpFilesystem.cc
# themselves, thus they link the plug-in without it.
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

add_unit_tests(filesystem
  SOURCES XrdClHttpFilesystem.cc
  LIBRARIES XrdClHttpWithoutFilesystem
)

# Exercise the actual worker with test-only aliases for the curl driver calls.
# Keep this object library separate so the installed plugin never contains the
# failure injection and there are no duplicate production symbols at link time.
add_library(XrdClHttpLifecycle OBJECT
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpHeaderBuilder.cc
  ${PROJECT_SOURCE_DIR}/src/XrdClHttp/XrdClHttpOptionsCache.cc
  WorkerLifecycleUtil.cc WorkerLifecycleOps.cc WorkerLifecycleOptions.cc)
target_link_libraries(XrdClHttpLifecycle PRIVATE
  XrdCl XrdUtils XrdXml CURL::libcurl OpenSSL::Crypto Threads::Threads)
target_include_directories(XrdClHttpLifecycle PRIVATE
  "${PROJECT_SOURCE_DIR}/src")

add_unit_tests(worker
  SOURCES XrdClHttpWorker.cc
  LIBRARIES XrdClHttpLifecycle XrdCl CURL::libcurl
  INCLUDE_DIRECTORIES ${PROJECT_SOURCE_DIR}/src
  PROPERTIES TIMEOUT 10
)
