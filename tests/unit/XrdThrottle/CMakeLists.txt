# The tested code links XrdServer, which a client-only build does not have
if(NOT TARGET XrdServer)
  return()
endif()

# Create a static library for testing (since MODULE libraries can't be linked)
add_library(XrdThrottleTestLib STATIC
  ${PROJECT_SOURCE_DIR}/src/XrdThrottle/XrdThrottleManager.cc
  ${PROJECT_SOURCE_DIR}/src/XrdThrottle/XrdThrottleConfig.cc
)

target_link_libraries(XrdThrottleTestLib
  PRIVATE
    XrdUtils
    XrdServer
    ${ATOMIC_LIBRARY}
)

target_include_directories(XrdThrottleTestLib
  PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/vendor/inih
)

add_unit_tests(
  SOURCES
    XrdThrottleUserLimits.cc
  LIBRARIES
    XrdThrottleTestLib
    XrdUtils
    XrdServer
)
