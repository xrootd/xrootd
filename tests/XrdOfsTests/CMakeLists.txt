add_executable(xrdofs-prep-gpi-tests
  XrdOfsPrepGPITests.cc
  ${PROJECT_SOURCE_DIR}/src/XrdOfs/XrdOfsPrepGPI.cc
)

target_compile_definitions(xrdofs-prep-gpi-tests PRIVATE XRD_PREP_GPI_TESTING)

target_link_libraries(xrdofs-prep-gpi-tests
  PRIVATE
    XrdUtils
    XrdServer
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(xrdofs-prep-gpi-tests
  TEST_PREFIX XrdOfsPrepGPI::
  DISCOVERY_TIMEOUT 10
  PROPERTIES TIMEOUT 30
)

if(BUILD_SCITOKENS)
  add_executable(xrdofs-verified-principal-tests VerifiedPrincipalTests.cc)
  target_include_directories(xrdofs-verified-principal-tests PRIVATE ${SCITOKENS_CPP_INCLUDE_DIR})
  target_link_libraries(xrdofs-verified-principal-tests PRIVATE XrdUtils GTest::gtest_main
    ${SCITOKENS_CPP_LIBRARIES} OpenSSL::Crypto ${CMAKE_DL_LIBS})
  target_compile_definitions(xrdofs-verified-principal-tests PRIVATE
    SCITOKENS_MODULE="$<TARGET_FILE:XrdAccSciTokens-${PLUGIN_VERSION}>")
  add_dependencies(xrdofs-verified-principal-tests XrdAccSciTokens-${PLUGIN_VERSION})
  add_dependencies(xrdofs-prep-gpi-tests xrdofs-verified-principal-tests)
  gtest_discover_tests(xrdofs-verified-principal-tests TEST_PREFIX XrdOfsPrepGPI::
    DISCOVERY_TIMEOUT 10 PROPERTIES TIMEOUT 30)
endif()

if(ENABLE_SERVER_TESTS)
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    add_library(XrdOfsPrepPathAuth MODULE PathAuth.cc)
    target_link_libraries(XrdOfsPrepPathAuth PRIVATE XrdUtils)
    add_test(NAME XrdOfsPrepGPI::PathAuthorization
      COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/authorization.py
        ${PROJECT_BINARY_DIR} $<TARGET_FILE:XrdOfsPrepPathAuth>)
    set_tests_properties(XrdOfsPrepGPI::PathAuthorization PROPERTIES TIMEOUT 60)
  endif()
endif()
