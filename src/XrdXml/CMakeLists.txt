add_library(XrdXml SHARED
  XrdXmlMetaLink.cc  XrdXmlMetaLink.hh
  XrdXmlRdrTiny.cc   XrdXmlRdrTiny.hh
  XrdXmlReader.cc    XrdXmlReader.hh
)

target_link_libraries(XrdXml PRIVATE XrdUtils ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(XrdXml PROPERTIES VERSION 3.0.0 SOVERSION 3)

if(TINYXML_FOUND)
  target_link_libraries(XrdXml PRIVATE ${TINYXML_LIBRARIES})
  target_include_directories(XrdXml PRIVATE ${TINYXML_INCLUDE_DIR})
else()
  target_sources(XrdXml
    PRIVATE
      tinyxml/tinystr.cpp       tinyxml/tinystr.h
      tinyxml/tinyxml.cpp       tinyxml/tinyxml.h
      tinyxml/tinyxmlerror.cpp
      tinyxml/tinyxmlparser.cpp
  )
  target_include_directories(XrdXml PRIVATE tinyxml)
endif()

if(LIBXML2_FOUND)
  target_sources(XrdXml
    PRIVATE
      XrdXmlRdrXml2.cc XrdXmlRdrXml2.hh
  )
  target_link_libraries(XrdXml PRIVATE ${LIBXML2_LIBRARIES})
  target_include_directories(XrdXml PRIVATE ${LIBXML2_INCLUDE_DIR})
endif()

install(TARGETS XrdXml LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
