add_library(XrdFfs SHARED
  XrdFfsDent.cc    XrdFfsDent.hh
  XrdFfsFsinfo.cc  XrdFfsFsinfo.hh
  XrdFfsMisc.cc    XrdFfsMisc.hh
  XrdFfsPosix.cc   XrdFfsPosix.hh
  XrdFfsQueue.cc   XrdFfsQueue.hh
  XrdFfsWcache.cc  XrdFfsWcache.hh
)

set_target_properties(XrdFfs
  PROPERTIES VERSION 3.0.0 SOVERSION 3)

target_link_libraries(XrdFfs
  PRIVATE
    XrdCl
    XrdPosix
    XrdUtils
    ${CMAKE_THREAD_LIBS_INIT}
)

install(TARGETS XrdFfs LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

# FUSE is only supported on Linux and GNU/FreeBSD

unset(BUILD_FUSE CACHE)

if(ENABLE_FUSE AND CMAKE_SYSTEM_NAME MATCHES "Linux|kFreeBSD")
  if(FORCE_ENABLED)
    find_package(fuse REQUIRED)
  else()
    find_package(fuse)
  endif()

  if(FUSE_FOUND)
    set(BUILD_FUSE TRUE CACHE INTERNAL "")
  else()
    return()
  endif()

  add_executable(xrootdfs XrdFfsXrootdfs.cc)

  target_link_libraries(xrootdfs
    XrdFfs
    XrdPosix
    ${FUSE_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
  )

  target_include_directories(xrootdfs PRIVATE ${FUSE_INCLUDE_DIR})

  install(TARGETS xrootdfs RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
