
add_executable(xrdsecssh-unit-tests
  XrdSecSSH.cc
)

target_link_libraries(xrdsecssh-unit-tests
  XrdUtils GTest::GTest GTest::Main OpenSSL::Crypto)

gtest_discover_tests(xrdsecssh-unit-tests
  PROPERTIES DISCOVERY_TIMEOUT 10)
