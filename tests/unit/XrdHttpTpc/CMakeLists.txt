add_library(XrdHttpTpcUtils
  ${PROJECT_SOURCE_DIR}/src/XrdHttpTpc/XrdHttpTpcUtils.cc
)

target_link_libraries(XrdHttpTpcUtils
        PUBLIC
        OpenSSL::SSL
        OpenSSL::Crypto)

add_unit_tests(SOURCES XrdHttpTpcUtils.cc LIBRARIES XrdHttpTpcUtils)

# The tested code links XrdServer, which a client-only build does not have
if(TARGET XrdServer)
  add_unit_tests(stream
    SOURCES
      XrdHttpTpcStream.cc
      ${PROJECT_SOURCE_DIR}/src/XrdHttpTpc/XrdHttpTpcStream.cc
    LIBRARIES
      XrdServer
      XrdUtils
  )
endif()
