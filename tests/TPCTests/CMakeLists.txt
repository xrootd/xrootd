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

  foreach(config srv1 srv2 common)
    configure_file(${config}.cfg ${CMAKE_CURRENT_BINARY_DIR}/${config}.cfg @ONLY)
  endforeach()

  add_test(NAME XRootD::tpc::start
    COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh start")
  set_tests_properties(XRootD::tpc::start
    PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_SETUP XRootD_TPC
               FIXTURES_REQUIRED "XRootD::scitokens;SciTokens")

  add_test(NAME XRootD::tpc::test
    COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/test.sh")
  set_tests_properties(XRootD::tpc::test
    PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_REQUIRED "XRootD_TPC;XRootD::scitokens;SciTokens")

  add_test(NAME XRootD::tpc::teardown
    COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh teardown")
  set_tests_properties(XRootD::tpc::teardown
    PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_CLEANUP XRootD_TPC)

endif()
