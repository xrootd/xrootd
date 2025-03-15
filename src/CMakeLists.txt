#-------------------------------------------------------------------------------
# If this is a build for pip set up the rpath
# (this is especially important for plugins)
#-------------------------------------------------------------------------------
if( PYPI_BUILD )
  set( CMAKE_INSTALL_RPATH "$ORIGIN" )
  set( CMAKE_INSTALL_RPATH_USE_LINK_PATH true )
endif()

#-------------------------------------------------------------------------------
# XRDCL_LIB_ONLY implies XRDCL_ONLY
#-------------------------------------------------------------------------------
if( XRDCL_LIB_ONLY )
  set ( XRDCL_ONLY TRUE )
endif()

include( XrdHeaders )

add_library(XrdUtils SHARED)

set_target_properties(XrdUtils
  PROPERTIES BUILD_RPATH $ORIGIN VERSION 3.0.0 SOVERSION 3
)

target_link_libraries(XrdUtils
  PRIVATE
    OpenSSL::SSL
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    ${SOCKET_LIBRARY}
    ${SENDFILE_LIBRARY}
    ${EXTRA_LIBS}
)

if(SYSTEMD_FOUND)
  target_link_libraries(XrdUtils PRIVATE ${SYSTEMD_LIBRARIES})
endif()

install(TARGETS XrdUtils LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

add_subdirectory( XProtocol )
add_subdirectory( Xrd )
add_subdirectory( XrdCks )
add_subdirectory( XrdNet )
add_subdirectory( XrdSut )
add_subdirectory( XrdSys )
add_subdirectory( XrdOuc )
add_subdirectory( XrdTls )
add_subdirectory( XrdRmc )

add_subdirectory( XrdApps )
add_subdirectory( XrdCrypto )
add_subdirectory( XrdPosix )

add_subdirectory( XrdSec )
add_subdirectory( XrdSecgsi )
add_subdirectory( XrdSeckrb5 )
add_subdirectory( XrdSecpwd )
add_subdirectory( XrdSecsss )
add_subdirectory( XrdSecunix )
add_subdirectory( XrdSecztn )

add_subdirectory( XrdXml )

add_subdirectory( XrdCl )
add_subdirectory( XrdClHttp )
add_subdirectory( XrdEc )

if( NOT XRDCL_ONLY )
  add_library(XrdServer SHARED)

  set_target_properties(XrdServer
    PROPERTIES VERSION 3.0.0 SOVERSION 3
  )

  target_link_libraries(XrdServer
    PRIVATE
    XrdUtils
    ${CMAKE_DL_LIBS}
    ${CMAKE_THREAD_LIBS_INIT}
    ${ATOMIC_LIBRARY}
    ${EXTRA_LIBS}
    ${SOCKET_LIBRARY}
  )

  install(TARGETS XrdServer LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

  add_subdirectory( XrdAcc )
  add_subdirectory( XrdCms )
  add_subdirectory( XrdDig )
  add_subdirectory( XrdFrc )
  add_subdirectory( XrdOfs )
  add_subdirectory( XrdOss )
  add_subdirectory( XrdSfs )
  add_subdirectory( XrdXrootd )

  add_subdirectory( XrdFrm )
  add_subdirectory( XrdFfs )
  add_subdirectory( XrdBwm )
  add_subdirectory( XrdPss )
  add_subdirectory( XrdSsi )
  add_subdirectory( XrdPfc )
  add_subdirectory( XrdOssCsi )
  add_subdirectory( XrdOssStats )
  add_subdirectory( XrdThrottle )

  add_subdirectory( XrdHttp )
  add_subdirectory( XrdHttpTpc )

  add_subdirectory( XrdMacaroons )
  add_subdirectory( XrdVoms )
  add_subdirectory( XrdCeph )
  add_subdirectory( XrdSciTokens )
endif()
