if(NOT TARGET XrdAccSciTokens-${PLUGIN_VERSION})
  return()
endif()

add_executable(xrdscitokens-integration-tests XrdSciTokensAccess.cc)
target_include_directories(xrdscitokens-integration-tests PRIVATE ${SCITOKENS_CPP_INCLUDE_DIR})
target_link_libraries(xrdscitokens-integration-tests PRIVATE
  XrdUtils GTest::gtest_main ${SCITOKENS_CPP_LIBRARIES} OpenSSL::Crypto ${CMAKE_DL_LIBS})
# std::filesystem is in a separate library before GCC 9.1.
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
  target_link_libraries(xrdscitokens-integration-tests PRIVATE stdc++fs)
endif()
target_compile_definitions(xrdscitokens-integration-tests PRIVATE
  SCITOKENS_MODULE="$<TARGET_FILE:XrdAccSciTokens-${PLUGIN_VERSION}>")
add_dependencies(xrdscitokens-integration-tests XrdAccSciTokens-${PLUGIN_VERSION})

gtest_discover_tests(xrdscitokens-integration-tests TEST_PREFIX integration.XrdSciTokens.
  DISCOVERY_TIMEOUT 10 PROPERTIES TIMEOUT 30)
