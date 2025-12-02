add_subdirectory(tinyxml)

add_library(XrdXml SHARED
  XrdXmlMetaLink.cc  XrdXmlMetaLink.hh
  XrdXmlRdrTiny.cc   XrdXmlRdrTiny.hh
  XrdXmlReader.cc    XrdXmlReader.hh
)

set_target_properties(XrdXml PROPERTIES VERSION 3.0.0 SOVERSION 3)

target_link_libraries(XrdXml
  PUBLIC
    XrdTinyXml
  PRIVATE
    XrdUtils
    ${CMAKE_THREAD_LIBS_INIT}
)

find_package(LibXml2)

if(LIBXML2_FOUND)
  target_sources(XrdXml
    PRIVATE
      XrdXmlRdrXml2.cc XrdXmlRdrXml2.hh
  )
  target_compile_definitions(XrdXml PRIVATE HAVE_XML2)
  target_link_libraries(XrdXml PRIVATE LibXml2::LibXml2)
endif()

install(TARGETS XrdXml LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
