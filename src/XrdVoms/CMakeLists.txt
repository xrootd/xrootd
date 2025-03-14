unset(BUILD_VOMS CACHE)

if(NOT ENABLE_VOMS)
  return()
endif()

if(FORCE_ENABLED)
  find_package(VOMS REQUIRED)
else()
  find_package(VOMS)
endif()

if(VOMS_FOUND)
  set(BUILD_VOMS TRUE CACHE INTERNAL "")
else()
  return()
endif()

set(XrdVoms       XrdVoms-${PLUGIN_VERSION})
set(XrdSecgsiVOMS XrdSecgsiVOMS-${PLUGIN_VERSION})
set(XrdHttpVOMS   XrdHttpVOMS-${PLUGIN_VERSION})

add_library(${XrdVoms} MODULE
  XrdVomsFun.cc
  XrdVomsMapfile.cc
  XrdVomsgsi.cc
  XrdVomsHttp.cc)

target_link_libraries(${XrdVoms} PRIVATE XrdCrypto ${VOMS_LIBRARIES})
target_include_directories(${XrdVoms} PRIVATE ${VOMS_INCLUDE_DIR})

install(TARGETS ${XrdVoms} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(
  CODE "
    EXECUTE_PROCESS(
      COMMAND ln -sf lib${XrdVoms}.so lib${XrdSecgsiVOMS}.so
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR} )" )

install(
  CODE "
    EXECUTE_PROCESS(
      COMMAND ln -sf lib${XrdVoms}.so lib${XrdHttpVOMS}.so
      WORKING_DIRECTORY \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR} )" )
