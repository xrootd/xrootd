set(XrdSecztnLib XrdSecztn-${PLUGIN_VERSION})

add_executable(xrdsecztn-token-tests XrdSecztnTokenTests.cc)

target_link_libraries(xrdsecztn-token-tests
  PRIVATE
    XrdUtils
    GTest::gtest
    GTest::gtest_main
    ${CMAKE_DL_LIBS}
)

target_compile_definitions(xrdsecztn-token-tests
  PRIVATE
    XRD_SECZTN_LIB="$<TARGET_FILE:${XrdSecztnLib}>"
)

add_dependencies(xrdsecztn-token-tests ${XrdSecztnLib})

gtest_discover_tests(xrdsecztn-token-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
