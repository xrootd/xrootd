# XrdClS3 tests depend on scitokens fixture for authentication
if(NOT BUILD_SCITOKENS)
  return()
endif()

include( GoogleTest )

add_executable( xrdcl-s3-test
  ReadTest.cc
  DeleteTest.cc
  DirListTest.cc
)

add_executable( xrdcl-s3-unittest
  UrlParseTest.cc
)

target_link_libraries( xrdcl-s3-test XrdClS3Testing XrdClCurlTransferTest GTest::gtest_main )

target_link_libraries( xrdcl-s3-unittest XrdClS3Testing GTest::gtest_main )

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
  AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
  target_link_libraries(xrdcl-s3-unittest stdc++fs)
endif()

gtest_add_tests( TARGET xrdcl-s3-test TEST_LIST S3Tests )
set_tests_properties( ${S3Tests}
  PROPERTIES
    FIXTURES_REQUIRED XrdClS3::s3
    ENVIRONMENT "XRD_LOGLEVEL=Debug;LSAN_OPTIONS=suppressions=${CMAKE_CURRENT_SOURCE_DIR}/leaksanitizer-suppress.txt;ENV_FILE=${CMAKE_BINARY_DIR}/tests/s3/setup.sh;XRD_PLUGINCONFDIR=${CMAKE_BINARY_DIR}/tests/s3/client.plugins.d;LD_LIBRARY_PATH=${XRootD_LIB_DIR}:$ENV{LD_LIBRARY_PATH}"
)

gtest_add_tests( TARGET xrdcl-s3-unittest TEST_LIST S3UnitTests )
set_tests_properties( ${S3UnitTests}
  PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=${XRootD_LIB_DIR}"
)

######################################
# Ensure minio is available
######################################
set(EXECUTABLES "minio" "mc")

foreach(EXE IN LISTS EXECUTABLES)

  find_program(EXE_BIN ${EXE} HINTS "${CMAKE_CURRENT_BINARY_DIR}")

  if(NOT EXE_BIN)
    message("Didnt find ${EXE}. Downloading it ...")

    if (APPLE)
      set(SYS_NAME "darwin")
    else()
      set(SYS_NAME "linux")
    endif()

    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" OR
       CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
      set(SYS_PROC "arm64")
    else()
      set(SYS_PROC "amd64")
    endif()

    if (EXE STREQUAL "minio")
      set(DIR "server")
    else()
      set(DIR "client")
    endif()

    set(URL "https://dl.min.io/${DIR}/${EXE}/release/${SYS_NAME}-${SYS_PROC}/${EXE}")
    set(FILE "${CMAKE_CURRENT_BINARY_DIR}/${EXE}")
    message("Downloading ${URL} to ${FILE}")
    file(DOWNLOAD "${URL}" "${FILE}" STATUS download_status)

    list(GET download_status 0 RESULT_CODE)
    if (NOT RESULT_CODE EQUAL 0)
      message(FATAL_ERROR "Failed to download ${EXE}: ${RESULT_CODE}")
    endif()

    file(CHMOD "${FILE}" PERMISSIONS
          OWNER_READ OWNER_WRITE OWNER_EXECUTE
          GROUP_READ GROUP_EXECUTE
          WORLD_READ WORLD_EXECUTE)
  endif()

endforeach()

find_program(MINIO_BIN minio HINTS "${CMAKE_CURRENT_BINARY_DIR}")
find_program(MC_BIN mc       HINTS "${CMAKE_CURRENT_BINARY_DIR}")

######################################
# Integration tests.
######################################
add_test(NAME XrdClS3::s3::setup
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/s3-setup.sh" s3)

set_tests_properties( XrdClS3::s3::setup
  PROPERTIES
    FIXTURES_SETUP XrdClS3::s3
    ENVIRONMENT "OPENSSL_BIN=${OPENSSL_BIN};BINARY_DIR=${CMAKE_BINARY_DIR};MC_BIN=${MC_BIN};MINIO_BIN=${MINIO_BIN};SOURCE_DIR=${CMAKE_SOURCE_DIR}"
)

add_test( NAME XrdClS3::s3::teardown
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/s3-teardown.sh" s3)

set_tests_properties( XrdClS3::s3::teardown
  PROPERTIES
    FIXTURES_CLEANUP XrdClS3::s3
    ENVIRONMENT "BINARY_DIR=${CMAKE_BINARY_DIR}"
)

add_test( NAME XrdClS3::s3::test
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/s3-test.sh" s3)

list( APPEND S3_TEST_LOGS ${CMAKE_CURRENT_BINARY_DIR}/tests/s3/minio.log )
list( APPEND S3_TEST_LOGS ${CMAKE_CURRENT_BINARY_DIR}/tests/s3/client.log )

set_tests_properties( XrdClS3::s3::test
  PROPERTIES
    FIXTURES_REQUIRED XrdClS3::s3
    ENVIRONMENT "BINARY_DIR=${CMAKE_BINARY_DIR}"
    ATTACHED_FILES_ON_FAIL "${S3_TEST_LOGS}"
)
