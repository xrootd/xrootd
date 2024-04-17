if( NOT XRDCL_LIB_ONLY )
  configure_file(xrootd-config xrootd-config @ONLY)
  install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/xrootd-config DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if( NOT XRDCL_ONLY )
  install(PROGRAMS
    ${CMAKE_SOURCE_DIR}/utils/XrdCmsNotify.pm
    ${CMAKE_SOURCE_DIR}/utils/netchk
    ${CMAKE_SOURCE_DIR}/utils/XrdOlbMonPerf
    ${CMAKE_SOURCE_DIR}/utils/cms_monPerf
    DESTINATION ${CMAKE_INSTALL_DATADIR}/xrootd/utils)
endif()
