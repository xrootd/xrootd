
list(APPEND XROOTD_CONFIGS noauth host unix sss)

if(ENABLE_FUSE_TESTS)
  list(APPEND XROOTD_CONFIGS fuse)
endif()

if(ENABLE_HTTP_TESTS)
  list(APPEND XROOTD_CONFIGS http)
endif()

foreach(CONFIG ${XROOTD_CONFIGS})
  add_test(NAME XRootD::${CONFIG}::setup
    COMMAND sh -c "${CMAKE_CURRENT_SOURCE_DIR}/test.sh ${CONFIG} setup")

  set_tests_properties(XRootD::${CONFIG}::setup
    PROPERTIES
      FIXTURES_SETUP XRootD::${CONFIG}
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

  set_tests_properties(XRootD::${CONFIG}::test
    PROPERTIES
      FIXTURES_REQUIRED XRootD::${CONFIG}
      ENVIRONMENT "BINARY_DIR=${CMAKE_BINARY_DIR}"
      ATTACHED_FILES_ON_FAIL "${${CONFIG}_LOGS}"
  )
endforeach()
