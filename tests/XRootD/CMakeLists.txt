
list(APPEND XROOTD_CONFIGS noauth gsi host unix sss cache posix diglib mirage moncollect)

# Tiny XrdCl driver that issues a vector read past EOF; the moncollect test uses
# it to provoke a terminal mid-transfer read error on the server's f-stream.
add_executable(xrdreadv-eof readv_eof.cc)
target_link_libraries(xrdreadv-eof PRIVATE XrdCl)

# Tiny XrdCl driver that opens a file for write twice; the second open is denied
# (kXR_FileLocked) before fp->open, exercising a failed-open report path in
# do_Open that bypasses fsError. Used by the moncollect test.
add_executable(xrdopen-denied open_denied.cc)
target_link_libraries(xrdopen-denied PRIVATE XrdCl)

if(ENABLE_FUSE_TESTS)
  list(APPEND XROOTD_CONFIGS fuse)
endif()

if(ENABLE_HTTP_TESTS)
  list(APPEND XROOTD_CONFIGS http tpcredir)
  # Test-only XrdXrootdRedirPI plugin loaded by tpcredir.cfg.
  add_subdirectory(redir_test)
endif()

if(BUILD_KRB5 AND NOT APPLE)
  list(APPEND XROOTD_CONFIGS krb5)
  list(APPEND krb5_FIXTURES Kerberos)
endif()

if(BUILD_MACAROONS)
  list(APPEND XROOTD_CONFIGS macaroons)
  list(APPEND macaroons_FIXTURES TLS)
endif()

# Note: HAVE_SCITOKEN_CONFIG_SET_STR is required to override the CA file
# for the scitokens library; otherwise, the test will fail with TLS errors.
if(BUILD_SCITOKENS AND HAVE_SCITOKEN_CONFIG_SET_STR)
  list(APPEND XROOTD_CONFIGS scitokens httpnoclient)
  list(APPEND scitokens_FIXTURES SciTokens)
  list(APPEND httpnoclient_FIXTURES SciTokens)
endif()

# The cache test requires the XRootD::host fixture to be running to
# act as an origin
list(APPEND cache_FIXTURES XRootD::host)

list(APPEND gsi_FIXTURES TLS)

# When the VOMS plug-in is built, the moncollect test additionally drives a gsi
# session whose proxy carries a (fake) VOMS attribute certificate, so it can
# assert that the VO is surfaced on the monitoring stream (user.vo). This needs
# the TLS fixture for certificates and a flag the shell uses to enable the gsi
# leg and mint the fake VOMS proxy (see moncollect.sh).
if(BUILD_VOMS)
  list(APPEND moncollect_FIXTURES TLS)
  set(moncollect_TEST_ENV "MONCOLLECT_VOMS=1")
endif()

foreach(CONFIG ${XROOTD_CONFIGS})
  # Base environment for every phase, plus any per-config extra (e.g. the
  # moncollect VOMS flag set above). Kept in a list so phases stay in sync.
  set(CONFIG_ENV "BINARY_DIR=${CMAKE_BINARY_DIR}")
  if(DEFINED ${CONFIG}_TEST_ENV)
    list(APPEND CONFIG_ENV "${${CONFIG}_TEST_ENV}")
  endif()

  add_test(NAME XRootD::${CONFIG}::setup
    COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CONFIG} setup")

  set_tests_properties(XRootD::${CONFIG}::setup
    PROPERTIES
      FIXTURES_SETUP XRootD::${CONFIG}
      FIXTURES_REQUIRED "${${CONFIG}_FIXTURES}"
      ENVIRONMENT "${CONFIG_ENV}"
  )

  add_test(NAME XRootD::${CONFIG}::teardown
    COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CONFIG} teardown")

  set_tests_properties(XRootD::${CONFIG}::teardown
    PROPERTIES
      FIXTURES_CLEANUP XRootD::${CONFIG}
      ENVIRONMENT "${CONFIG_ENV}"
  )

  add_test(NAME XRootD::${CONFIG}::test
    COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CONFIG} run")

  list(APPEND ${CONFIG}_LOGS ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG}/client.log)
  list(APPEND ${CONFIG}_LOGS ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG}/xrootd.log)

  list(APPEND ${CONFIG}_FIXTURES XRootD::${CONFIG})

  set_tests_properties(XRootD::${CONFIG}::test
    PROPERTIES
      FIXTURES_REQUIRED "${${CONFIG}_FIXTURES}"
      ENVIRONMENT "${CONFIG_ENV}"
      ATTACHED_FILES_ON_FAIL "${${CONFIG}_LOGS}"
  )
endforeach()

if(BUILD_SCITOKENS AND HAVE_SCITOKEN_CONFIG_SET_STR)
  set_tests_properties(XRootD::scitokens::setup
    PROPERTIES
      FIXTURES_REQUIRED SciTokens
  )
  set_tests_properties(XRootD::httpnoclient::setup
    PROPERTIES
      FIXTURES_REQUIRED SciTokens
  )
endif()
