if( NOT XRDCL_LIB_ONLY )
  configure_file(xrootd-config xrootd-config @ONLY)
  install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/xrootd-config DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if( NOT XRDCL_ONLY )
  install(PROGRAMS
    XrdCmsNotify.pm
    netchk
    XrdOlbMonPerf
    cms_monPerf
    xrdcp-tpc.sh
    DESTINATION ${CMAKE_INSTALL_DATADIR}/xrootd/utils)
endif()
