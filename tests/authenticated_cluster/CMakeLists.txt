if (BUILD_SCITOKENS AND HAVE_SCITOKEN_CONFIG_SET_STR)

list(APPEND XRDENV "XRDCP=$<TARGET_FILE:xrdcp>")
list(APPEND XRDENV "XRDFS=$<TARGET_FILE:xrdfs>")
list(APPEND XRDENV "CRC32C=$<TARGET_FILE:xrdcrc32c>")
list(APPEND XRDENV "ADLER32=$<TARGET_FILE:xrdadler32>")
list(APPEND XRDENV "XROOTD=$<TARGET_FILE:xrootd>")
list(APPEND XRDENV "CMSD=$<TARGET_FILE:cmsd>")
list(APPEND XRDENV "XRDSCITOKENS_CREATE_TOKEN=${CMAKE_BINARY_DIR}/bin/xrdscitokens-create-token")
list(APPEND XRDENV "XRDSCITOKENS_ISSUER_DIR=${CMAKE_BINARY_DIR}/tests/issuer")
list(APPEND XRDENV "X509_CERT_FILE=${CMAKE_BINARY_DIR}/tests/issuer/tlsca.pem")
list(APPEND XRDENV "BINARY_DIR=${CMAKE_BINARY_DIR}")
list(APPEND XRDENV "XDG_CACHE_HOME=${CMAKE_CURRENT_BINARY_DIR}/.cache")

foreach(config common metaman man1 man2 srv1 srv2 srv3 srv4)
  configure_file(${config}.cfg ${config}.cfg @ONLY)
endforeach()

add_test(NAME XRootD::authenticated_cluster::start
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh start")
set_tests_properties(XRootD::authenticated_cluster::start
  PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_SETUP XRootD_Authenticated_Cluster FIXTURES_REQUIRED "XRootD::scitokens;SciTokens")

add_test(NAME XRootD::authenticated_cluster::test
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/test.sh")
set_tests_properties(XRootD::authenticated_cluster::test
  PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_REQUIRED "XRootD_Authenticated_Cluster;XRootD::scitokens;SciTokens")

add_test(NAME XRootD::authenticated_cluster::stop
  COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh stop")
set_tests_properties(XRootD::authenticated_cluster::stop
  PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_CLEANUP XRootD_Authenticated_Cluster)

endif()
