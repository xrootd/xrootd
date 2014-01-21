#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_HTTP_VERSION   1.0.0 )
set( XRD_HTTP_SOVERSION 1 )



# -------------------------
# Local targets
# -------------------------

set(XrdHttp_SOURCES XrdHTTP/XrdHttpProtocol.cc XrdHTTP/XrdHttpReq.cc XrdHTTP/XrdHttpTrace.cc XrdHTTP/XrdHttpUtils.cc)

#
# Our target is a library to be loaded as a plugin by Xrootd
#
add_library(XrdHttp SHARED ${XrdHttp_SOURCES})
set_target_properties(XrdHttp PROPERTIES
 VERSION "${XRD_HTTP_VERSION}"
 SOVERSION "${XRD_HTTP_SOVERSION}")

target_link_libraries(XrdHttp XrdServer XrdXrootd XrdUtils XrdCrypto dl pthread ssl crypto )

set_target_properties(
  XrdHttp
  PROPERTIES
  VERSION   ${XRD_HTTP_VERSION}
  SOVERSION ${XRD_HTTP_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

# Install directive.
install(TARGETS XrdHttp
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )

