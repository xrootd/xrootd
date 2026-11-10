if(NOT BUILD_CEPH)
  return()
endif()

add_unit_tests(SOURCES XrdCephPosix.cc LIBRARIES XrdCephPosix)
