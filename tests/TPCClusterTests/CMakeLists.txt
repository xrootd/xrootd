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

  foreach(config common man1 srv1 srv2)
    configure_file(${config}.cfg ${config}.cfg @ONLY)
  endforeach()

  # Named to contain "XRootD::tpc::test" so that -R on it selects this suite too.
  add_test(NAME XRootD::tpc::test::cluster::start
    COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh start")
  set_tests_properties(XRootD::tpc::test::cluster::start
    PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_SETUP XRootD_TPC_Cluster
               FIXTURES_REQUIRED "XRootD::scitokens;SciTokens")

  add_test(NAME XRootD::tpc::test::cluster
    COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/test.sh")
  set_tests_properties(XRootD::tpc::test::cluster
    PROPERTIES ENVIRONMENT "${XRDENV}"
               FIXTURES_REQUIRED "XRootD_TPC_Cluster;XRootD::scitokens;SciTokens")

  add_test(NAME XRootD::tpc::test::cluster::stop
    COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/setup.sh stop")
  set_tests_properties(XRootD::tpc::test::cluster::stop
    PROPERTIES ENVIRONMENT "${XRDENV}" FIXTURES_CLEANUP XRootD_TPC_Cluster)

endif()
