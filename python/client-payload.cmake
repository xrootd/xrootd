# Reuse the native clients, client libraries and dynamically loaded plugins.
# The same staging commands are used by setup.py and the enclosing CMake wheel.

set(PYXROOTD_COMMANDS)

set(PYXROOTD_PAYLOAD_TARGETS client)
set(PYXROOTD_PAYLOAD_FILES)

if(TARGET xrdfs AND TARGET xrdcp)
  set(PYXROOTD_COMMANDS xrdfs xrdcp xrdcopy)
  list(APPEND PYXROOTD_PAYLOAD_TARGETS xrdfs xrdcp
    XrdCl XrdUtils XrdAppUtils XrdXml XrdCrypto XrdCryptoLite)

  # Authentication and checksum plugins are loaded dynamically, so they do
  # not appear in the executables' linked-library dependencies.
  foreach(plugin XrdSec XrdSecProt XrdSecgsi XrdSecpwd XrdSecsss XrdSecunix
                 XrdSecztn XrdCryptossl XrdCksCalczcrc32)
    list(APPEND PYXROOTD_PAYLOAD_TARGETS ${plugin}-${PLUGIN_VERSION})
  endforeach()
  foreach(plugin XrdClHttp XrdSeckrb5)
    if(TARGET ${plugin}-${PLUGIN_VERSION})
      list(APPEND PYXROOTD_PAYLOAD_TARGETS ${plugin}-${PLUGIN_VERSION})
    endif()
  endforeach()
  if(APPLE)
    # XrdCl currently preloads these GSI helpers at startup on macOS.
    list(APPEND PYXROOTD_PAYLOAD_TARGETS
      XrdSecgsiAUTHZVO-${PLUGIN_VERSION} XrdSecgsiGMAPDN-${PLUGIN_VERSION})
  endif()
endif()

if(PYPI_BUILD AND (NOT PYXROOTD_COMMANDS OR NOT TARGET XrdClHttp-${PLUGIN_VERSION}))
  message(FATAL_ERROR "PyPI builds require native clients and the HTTP plugin")
endif()

set(PYXROOTD_PAYLOAD_DIR "${CMAKE_CURRENT_BINARY_DIR}/extension"
    CACHE PATH "Staging directory for the Python wheel native client payload")

set(PYXROOTD_PAYLOAD_COMMANDS
  COMMAND ${CMAKE_COMMAND} -E rm -rf ${PYXROOTD_PAYLOAD_DIR}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PYXROOTD_PAYLOAD_DIR})

if(PYXROOTD_COMMANDS AND TARGET XrdClHttp-${PLUGIN_VERSION})
  file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/http.conf CONTENT
    "url = http://*;https://*;dav://*;davs://*\nlib = $<TARGET_FILE_NAME:XrdClHttp-${PLUGIN_VERSION}>\nenable = true\n")
  list(APPEND PYXROOTD_PAYLOAD_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_BINARY_DIR}/http.conf ${PYXROOTD_PAYLOAD_DIR})
  list(APPEND PYXROOTD_PAYLOAD_FILES ${CMAKE_CURRENT_BINARY_DIR}/http.conf)
endif()

if(PYXROOTD_COMMANDS)
  # The enclosing wheel copies build artifacts. Add sibling lookup there;
  # regular native installations retain their original install RPATH.
  # PyPI builds already use the sibling install RPATH from src/CMakeLists.
  if(APPLE)
    set(relative_rpath "@loader_path")
  else()
    set(relative_rpath "\$ORIGIN")
  endif()
  set_property(TARGET ${PYXROOTD_PAYLOAD_TARGETS}
    APPEND PROPERTY BUILD_RPATH ${relative_rpath})
endif()

foreach(target IN LISTS PYXROOTD_PAYLOAD_TARGETS)
  get_target_property(type ${target} TYPE)
  if(type STREQUAL "SHARED_LIBRARY")
    # Preserve the SONAME needed by the loader and the unversioned name used
    # by dlopen. Copies avoid symlinks, which wheel ZIPs cannot preserve.
    list(APPEND PYXROOTD_PAYLOAD_COMMANDS
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target}>
              ${PYXROOTD_PAYLOAD_DIR}/$<TARGET_SONAME_FILE_NAME:${target}>
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target}>
              ${PYXROOTD_PAYLOAD_DIR}/$<TARGET_LINKER_FILE_NAME:${target}>)
  else()
    list(APPEND PYXROOTD_PAYLOAD_COMMANDS
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${target}>
              ${PYXROOTD_PAYLOAD_DIR})
  endif()
endforeach()

# Source-package builds invoke this target explicitly. The enclosing CMake
# wheel stages within its output rule, so an unchanged build does no copying.
if(PYPI_BUILD)
  add_custom_target(PyXRootDClientPayload
    ${PYXROOTD_PAYLOAD_COMMANDS}
    DEPENDS ${PYXROOTD_PAYLOAD_TARGETS} ${PYXROOTD_PAYLOAD_FILES}
    COMMENT "Staging Python wheel native clients and plugins"
    VERBATIM)
endif()

# setup.py needs a space-separated list when substituted into Python source.
string(JOIN " " PYXROOTD_COMMANDS ${PYXROOTD_COMMANDS})
