if(NOT ENABLE_CEPH)
  unset(BUILD_CEPH CACHE)
  return()
endif()

if(FORCE_ENABLED)
  find_package(ceph REQUIRED)
else()
  find_package(ceph)
  if(NOT CEPH_FOUND)
    unset(BUILD_CEPH CACHE)
    return()
  endif()
endif()

set(BUILD_CEPH TRUE CACHE BOOL INTERNAL FORCE)

add_library(XrdCephPosix SHARED
  XrdCephPosix.cc XrdCephPosix.hh)

target_compile_options(XrdCephPosix
  PRIVATE -Wno-deprecated-declarations)

target_link_libraries(XrdCephPosix
  PRIVATE XrdUtils ${RADOS_LIBS})

target_include_directories(XrdCephPosix
  PUBLIC ${RADOS_INCLUDE_DIR} $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>)

set_target_properties(XrdCephPosix
  PROPERTIES VERSION 0.0.1 SOVERSION 0)

set(LIB_XRD_CEPH XrdCeph-${PLUGIN_VERSION})

add_library(${LIB_XRD_CEPH} MODULE
  XrdCephOss.cc     XrdCephOss.hh
  XrdCephOssFile.cc XrdCephOssFile.hh
  XrdCephOssDir.cc  XrdCephOssDir.hh)

target_link_libraries(${LIB_XRD_CEPH}
  PRIVATE ${XROOTD_LIBRARIES} XrdCephPosix)

set(LIB_XRD_CEPH_XATTR XrdCephXattr-${PLUGIN_VERSION})

add_library(${LIB_XRD_CEPH_XATTR} MODULE
  XrdCephXAttr.cc XrdCephXAttr.hh)

target_link_libraries(${LIB_XRD_CEPH_XATTR}
  PRIVATE ${XROOTD_LIBRARIES} XrdCephPosix)

install(TARGETS XrdCephPosix ${LIB_XRD_CEPH} ${LIB_XRD_CEPH_XATTR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
