# XrdClS3 tests depend on scitokens fixture for authentication
if(NOT BUILD_SCITOKENS)
  return()
endif()

# minio/mc are only available for Linux/macOS on certain architectures
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|x86_64)$")
  message(STATUS "Skipping XrdClS3 tests, unsupported architecture")
  return()
endif()

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

foreach(EXE minio mc)
  find_program(EXE_BIN ${EXE} HINTS "${CMAKE_CURRENT_BINARY_DIR}")

  if(NOT EXE_BIN)
    message(STATUS "Could not find ${EXE}. Attempting to download it.")

    if (EXE STREQUAL "minio")
      set(DIR "server")
    else()
      set(DIR "client")
    endif()

    set(URL "https://dl.min.io/${DIR}/${EXE}/release/${SYS_NAME}-${SYS_PROC}/${EXE}")
    set(FILE "${CMAKE_CURRENT_BINARY_DIR}/${EXE}")
    message(STATUS "Downloading ${URL} to ${FILE}")
    file(DOWNLOAD "${URL}" "${FILE}" STATUS download_status)

    list(GET download_status 0 RESULT_CODE)
    list(GET download_status 1 RESULT_REASON)
    if (NOT RESULT_CODE EQUAL 0)
      message(STATUS "Could not download ${EXE}: ${RESULT_REASON}, skipping XrdClS3 tests")
      return()
    endif()

    file(CHMOD "${FILE}" PERMISSIONS
          OWNER_READ OWNER_WRITE OWNER_EXECUTE
          GROUP_READ GROUP_EXECUTE
          WORLD_READ WORLD_EXECUTE)
  endif()
endforeach()

find_program(MINIO_BIN minio HINTS "${CMAKE_CURRENT_BINARY_DIR}")
find_program(MC_BIN mc       HINTS "${CMAKE_CURRENT_BINARY_DIR}")

include(GoogleTest)

add_executable(xrdcl-s3-test
  ReadTest.cc
  DeleteTest.cc
  DirListTest.cc
)

add_executable(xrdcl-s3-unittest
  UrlParseTest.cc
)

target_link_libraries(xrdcl-s3-test XrdClS3Obj XrdClCurlTransferTest GTest::gtest_main)
target_link_libraries(xrdcl-s3-unittest XrdClS3Obj GTest::gtest_main)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
  AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
  target_link_libraries(xrdcl-s3-unittest stdc++fs)
endif()

gtest_add_tests(TARGET xrdcl-s3-test TEST_LIST S3Tests)
set_tests_properties(${S3Tests}
  PROPERTIES
    FIXTURES_REQUIRED XrdClS3::s3
    ENVIRONMENT "XRD_LOGLEVEL=Debug;ENV_FILE=${CMAKE_BINARY_DIR}/tests/s3/setup.sh;XRD_PLUGINCONFDIR=${CMAKE_BINARY_DIR}/tests/s3/client.plugins.d"
)

######################################
# Integration tests.
######################################
add_test(NAME XrdClS3::s3::setup
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/s3-setup.sh" s3)

set_tests_properties(XrdClS3::s3::setup
  PROPERTIES
    FIXTURES_SETUP XrdClS3::s3
    ENVIRONMENT "OPENSSL_BIN=${OPENSSL_BIN};BINARY_DIR=${CMAKE_BINARY_DIR};MC_BIN=${MC_BIN};MINIO_BIN=${MINIO_BIN};SOURCE_DIR=${CMAKE_SOURCE_DIR}"
)

add_test(NAME XrdClS3::s3::teardown
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/s3-teardown.sh" s3)

set_tests_properties(XrdClS3::s3::teardown
  PROPERTIES
    FIXTURES_CLEANUP XrdClS3::s3
    ENVIRONMENT "BINARY_DIR=${CMAKE_BINARY_DIR}"
)

add_test(NAME XrdClS3::s3::test
  COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/s3-test.sh" s3)

list(APPEND S3_TEST_LOGS ${CMAKE_CURRENT_BINARY_DIR}/tests/s3/minio.log)
list(APPEND S3_TEST_LOGS ${CMAKE_CURRENT_BINARY_DIR}/tests/s3/client.log)

set_tests_properties(XrdClS3::s3::test
  PROPERTIES
    FIXTURES_REQUIRED XrdClS3::s3
    ENVIRONMENT "BINARY_DIR=${CMAKE_BINARY_DIR}"
    ATTACHED_FILES_ON_FAIL "${S3_TEST_LOGS}"
)
