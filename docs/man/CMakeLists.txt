if( XRDCL_LIB_ONLY )
  return()
endif()

set(MAN1PAGES
  xrdcp.1
  xrdfs.1
  xrdmapc.1
)

set(MAN8PAGES
    cmsd.8
    frm_admin.8
    frm_purged.8
    frm_xfragent.8
    frm_xfrd.8
    mpxstats.8
    xrdpfc_print.8
    xrdpwdadmin.8
    xrdsssadmin.8
    xrootd.8
)

if ( BUILD_FUSE )
  list(APPEND MAN1PAGES xrootdfs.1)
endif()

if ( NOT XRDCL_ONLY )
  list(APPEND MAN1PAGES xrdadler32.1 xrdgsiproxy.1 xrdgsitest.1)

  foreach(MAN ${MAN8PAGES})
    configure_file(${MAN} man8/${MAN} @ONLY)
  endforeach()

  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/man8
          DESTINATION ${CMAKE_INSTALL_MANDIR})

  if( BUILD_VOMS )
    list(APPEND MAN1PAGES libXrdVoms.1)
  endif()
endif()

foreach(MAN ${MAN1PAGES})
  configure_file(${MAN} man1/${MAN} @ONLY)
endforeach()

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/man1
        DESTINATION ${CMAKE_INSTALL_MANDIR})

install(CODE
  "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink xrdcp.1 xrdcopy.1
     WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}/man1)")

if( BUILD_VOMS )
  install(CODE
    "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink libXrdVoms.1 libXrdSecgsiVOMS.1
       WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_MANDIR}/man1)")
endif()
