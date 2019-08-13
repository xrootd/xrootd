
if( READLINE_FOUND )
  include_directories( ${READLINE_INCLUDE_DIR} )
endif()

include_directories( ../ ./ ${ZLIB_INCLUDE_DIRS} ${UUID_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src
  /usr/local/include )
