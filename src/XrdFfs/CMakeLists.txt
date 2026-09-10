add_library(XrdFfs SHARED
  XrdFfsDent.cc    XrdFfsDent.hh
  XrdFfsFsinfo.cc  XrdFfsFsinfo.hh
  XrdFfsMisc.cc    XrdFfsMisc.hh
  XrdFfsPosix.cc   XrdFfsPosix.hh
  XrdFfsQueue.cc   XrdFfsQueue.hh
  XrdFfsWcache.cc  XrdFfsWcache.hh
)

set_target_properties(XrdFfs
  PROPERTIES
    SOVERSION ${XRootD_VERSION_MAJOR}
    VERSION ${XRootD_LIBVERSION}
)

target_link_libraries(XrdFfs
  PRIVATE
    XrdCl
    XrdPosix
    XrdUtils
    ${CMAKE_THREAD_LIBS_INIT}
)

install(TARGETS XrdFfs LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# FUSE is only supported on Linux and GNU/FreeBSD
#
# Converted from FUSE 2.x to FUSE 3.x: Xrootd's find_package(fuse) module
# (cmake/FindFUSE.cmake) resolves to libfuse 2.x and there is no bundled
# FindFUSE3.cmake, so FUSE 3.x is located via pkg-config's fuse3.pc instead.
# This requires fuse3-devel / libfuse3-dev to be installed on the build host.

unset(BUILD_FUSE CACHE)

if(ENABLE_FUSE AND CMAKE_SYSTEM_NAME MATCHES "Linux|kFreeBSD")
  find_package(PkgConfig REQUIRED)
  if(FORCE_ENABLED)
    pkg_check_modules(FUSE3 REQUIRED fuse3)
  else()
    pkg_check_modules(FUSE3 fuse3)
  endif()

  if(FUSE3_FOUND)
    set(BUILD_FUSE TRUE CACHE INTERNAL "")
  else()
    return()
  endif()

  add_executable(xrootdfs XrdFfsXrootdfs.cc)

  target_link_libraries(xrootdfs
    XrdFfs
    XrdPosix
    ${FUSE3_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
  )

  target_include_directories(xrootdfs PRIVATE ${FUSE3_INCLUDE_DIRS})
  target_compile_options(xrootdfs PRIVATE ${FUSE3_CFLAGS_OTHER})

  install(TARGETS xrootdfs RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
