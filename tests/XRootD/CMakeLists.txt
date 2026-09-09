
list(APPEND XROOTD_CONFIGS noauth gsi host unix sss cache posix diglib mirage)

# XCache (XrdPfc) configurations. Each fronts the XRootD::host fixture as its
# origin and exercises one aspect of the cache; the shared shell helpers are in
# cachelib.sh.
list(APPEND XROOTD_CONFIGS cacheevict cacheprefetch cacheblacklist cachespaces cacherestart)

# cachepurge is slow on purpose: the purge check runs on a fixed 60 s cycle and
# there is no way to trigger one, so the test has to wait for at least one pass.
list(APPEND XROOTD_CONFIGS cachepurge)

# Range-reading client for the cache tests. xrdcp only does whole files, and
# the interesting XCache behaviour is block-granular. Threads::Threads because
# the concurrent-reader mode uses std::thread, which needs pthread linked
# explicitly on glibc older than 2.34.
add_executable(xrdpfc-read utils/xrdpfc-read.cc)
target_link_libraries(xrdpfc-read XrdCl XrdUtils Threads::Threads)

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

# The cache tests require the XRootD::host fixture to be running to
# act as an origin
foreach(CACHE_CONFIG cache cacheevict cacheprefetch cacheblacklist cachespaces cacherestart cachepurge)
  list(APPEND ${CACHE_CONFIG}_FIXTURES XRootD::host)
endforeach()

list(APPEND gsi_FIXTURES TLS)

foreach(CONFIG ${XROOTD_CONFIGS})
  add_test(NAME XRootD::${CONFIG}::setup
    COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CONFIG} setup")

  set_tests_properties(XRootD::${CONFIG}::setup
    PROPERTIES
      FIXTURES_SETUP XRootD::${CONFIG}
      FIXTURES_REQUIRED "${${CONFIG}_FIXTURES}"
      ENVIRONMENT "BINARY_DIR=${CMAKE_BINARY_DIR}"
  )

  add_test(NAME XRootD::${CONFIG}::teardown
    COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CONFIG} teardown")

  set_tests_properties(XRootD::${CONFIG}::teardown
    PROPERTIES
      FIXTURES_CLEANUP XRootD::${CONFIG}
      ENVIRONMENT "BINARY_DIR=${CMAKE_BINARY_DIR}"
  )

  add_test(NAME XRootD::${CONFIG}::test
    COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CONFIG} run")

  list(APPEND ${CONFIG}_LOGS ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG}/client.log)
  list(APPEND ${CONFIG}_LOGS ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG}/xrootd.log)

  list(APPEND ${CONFIG}_FIXTURES XRootD::${CONFIG})

  set_tests_properties(XRootD::${CONFIG}::test
    PROPERTIES
      FIXTURES_REQUIRED "${${CONFIG}_FIXTURES}"
      ENVIRONMENT "BINARY_DIR=${CMAKE_BINARY_DIR}"
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

# Waiting out purge cycles, see cachepurge.sh.
set_tests_properties(XRootD::cachepurge::test PROPERTIES TIMEOUT 600 LABELS slow)
