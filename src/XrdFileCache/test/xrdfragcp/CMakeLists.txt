
include_directories( ${XROOTD_INCLUDES} )
add_executable( xrdfragcp xrdfragcp.cxx )
add_executable( xrdreadv xrdreadv.cxx )

target_link_libraries( xrdfragcp ${XROOTD_UTILS} ${XROOTD_CLIENT} )
target_link_libraries( xrdreadv ${XROOTD_UTILS} ${XROOTD_CLIENT} )

install(
  PROGRAMS xrdfragcp
  DESTINATION bin)

install(
  PROGRAMS xrdreadv
  DESTINATION bin)

