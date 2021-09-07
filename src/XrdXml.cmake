
include( XRootDCommon )

if ( TINYXML_FOUND )
   set( TINYXML_FILES "" )
   set( TINYXML_LIBRARIES ${TINYXML_LIBRARIES} )
else()
   set( TINYXML_FILES
        XrdXml/tinyxml/tinystr.cpp       XrdXml/tinyxml/tinystr.h
        XrdXml/tinyxml/tinyxml.cpp       XrdXml/tinyxml/tinyxml.h
        XrdXml/tinyxml/tinyxmlerror.cpp
        XrdXml/tinyxml/tinyxmlparser.cpp )
   set( TINYXML_LIBRARIES "" )
endif()

if ( LIBXML2_FOUND )
   set( XRDXML2_READER_FILES
        XrdXml/XrdXmlRdrXml2.cc
        XrdXml/XrdXmlRdrXml2.hh )
   set( XRDXML2_LIBRARIES ${LIBXML2_LIBRARIES} )
else()
   set( XRDXML2_READER_FILES "" )
   set( XRDXML2_LIBRARIES "" )
endif()

#-------------------------------------------------------------------------------
# Shared library version
#-------------------------------------------------------------------------------
set( XRD_XML_VERSION   3.0.0 )
set( XRD_XML_SOVERSION 3 )
set( XRD_XML_PRELOAD_VERSION   2.0.0 )
set( XRD_XML_PRELOAD_SOVERSION 2 )

#-------------------------------------------------------------------------------
# The XrdXml library
#-------------------------------------------------------------------------------
add_library(
  XrdXml
  SHARED
  ${TINYXML_FILES}
  XrdXml/XrdXmlMetaLink.cc         XrdXml/XrdXmlMetaLink.hh
  XrdXml/XrdXmlRdrTiny.cc          XrdXml/XrdXmlRdrTiny.hh
  XrdXml/XrdXmlReader.cc           XrdXml/XrdXmlReader.hh
  ${XRDXML2_READER_FILES} )

target_link_libraries(
  XrdXml
  XrdUtils
  ${TINYXML_LIBRARIES}
  ${XRDXML2_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT} )

set_target_properties(
  XrdXml
  PROPERTIES
  VERSION   ${XRD_XML_VERSION}
  SOVERSION ${XRD_XML_SOVERSION}
  INTERFACE_LINK_LIBRARIES ""
  LINK_INTERFACE_LIBRARIES "" )

if ( TINYXML_FOUND )
   target_include_directories( XrdXml PRIVATE ${TINYXML_INCLUDE_DIR} )
else()
   target_include_directories( XrdXml PRIVATE XrdXml/tinyxml )
endif()

if ( LIBXML2_FOUND )
   target_include_directories( XrdXml PRIVATE ${LIBXML2_INCLUDE_DIR} )
endif()

#-------------------------------------------------------------------------------
# Install
#-------------------------------------------------------------------------------
install(
  TARGETS XrdXml
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
