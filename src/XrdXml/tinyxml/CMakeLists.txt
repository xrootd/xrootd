add_library(XrdTinyXml OBJECT
  tinystr.cpp       tinystr.h
  tinyxml.cpp       tinyxml.h
  tinyxmlerror.cpp
  tinyxmlparser.cpp
)

set_target_properties(XrdTinyXml
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(XrdTinyXml
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
