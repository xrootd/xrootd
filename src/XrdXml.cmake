
include( XRootDCommon )

if ( LIBXML2_FOUND )
   set( XRDXML2_READER_FILES
        XrdXml/XrdXmlRdrXml2.cc
        XrdXml/XrdXmlRdrXml2.hh )
   set( XRDXML2_LIBRARIES ${LIBXML2_LIBRARIES} )
   if( CMAKE_VERSION VERSION_LESS "2.8" OR ${Solaris} STREQUAL "TRUE")
      INCLUDE_DIRECTORIES( ${LIBXML2_INCLUDE_DIR} )
   endif()
else()
   set( XRDXML2_READER_FILES "" )
   set( XRDXML2_LIBRARIES "" )
endif()

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_XML_VERSION   2.0.0 )
set( XRD_XML_SOVERSION 2 )
set( XRD_XML_PRELOAD_VERSION   1.0.0 )
set( XRD_XML_PRELOAD_SOVERSION 1 )

#-------------------------------------------------------------------------------
# The XrdXml library
#-------------------------------------------------------------------------------
add_library(
  XrdXml
  SHARED
  XrdXml/tinystr.cpp               XrdXml/tinystr.h
  XrdXml/tinyxml.cpp               XrdXml/tinyxml.h
  XrdXml/tinyxmlerror.cpp
  XrdXml/tinyxmlparser.cpp
  XrdXml/XrdXmlMetaLink.cc         XrdXml/XrdXmlMetaLink.hh
  XrdXml/XrdXmlRdrTiny.cc          XrdXml/XrdXmlRdrTiny.hh
  XrdXml/XrdXmlReader.cc           XrdXml/XrdXmlReader.hh
  ${XRDXML2_READER_FILES} )

target_link_libraries(
  XrdXml
  XrdUtils
  ${XRDXML2_LIBRARIES}
  pthread )
# INCLUDE_DIRECTORIES /usr/include/libxml2

set_target_properties(
  XrdXml
  PROPERTIES
  INCLUDE_DIRECTORIES ${CMAKE_SOURCE_DIR}/src/XrdXml/.
  VERSION   ${XRD_XML_VERSION}
  SOVERSION ${XRD_XML_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

if ( LIBXML2_FOUND )
   set_target_properties(
        XrdXml
        PROPERTIES
        INCLUDE_DIRECTORIES ${LIBXML2_INCLUDE_DIR} )
endif()

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdXml
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
