#.rst:
#
# XRootDVersion
# -------------
#
# This module sets the version of XRootD.
#
# The version is determined in the following order:
#  * If a version is set with -DXRootD_VERSION_STRING=x.y.z during configuration, it is used.
#  * The version is read from the 'VERSION' file at the top directory of the repository.
#  * If the 'VERSION' file has not been expanded, a version is set using git describe.
#  * If none of the above worked, a fallback version is set using the current date.
#

if(NOT DEFINED XRootD_VERSION_STRING)
  file(READ "${PROJECT_SOURCE_DIR}/VERSION" XRootD_VERSION_STRING)
  string(STRIP ${XRootD_VERSION_STRING} XRootD_VERSION_STRING)
endif()

if(XRootD_VERSION_STRING MATCHES "Format:" AND IS_DIRECTORY ${PROJECT_SOURCE_DIR}/.git)
  find_package(Git QUIET)
  if(Git_FOUND)
    message(VERBOSE "Determining version with git")
    execute_process(COMMAND
      ${GIT_EXECUTABLE} --git-dir ${PROJECT_SOURCE_DIR}/.git describe
      OUTPUT_VARIABLE XRootD_VERSION_STRING ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(XRootD_VERSION_STRING "${XRootD_VERSION_STRING}" CACHE INTERNAL "XRootD Version")
  endif()
endif()

if(XRootD_VERSION_STRING MATCHES "^v?([0-9]+)[.]*([0-9]*)[.]*([0-9]*)[.]*([0-9]*)")
  set(XRootD_VERSION_MAJOR ${CMAKE_MATCH_1})
  if(${CMAKE_MATCH_COUNT} GREATER 1)
    set(XRootD_VERSION_MINOR ${CMAKE_MATCH_2})
  else()
    set(XRootD_VERSION_MINOR 0)
  endif()
  if(${CMAKE_MATCH_COUNT} GREATER 2)
    set(XRootD_VERSION_PATCH ${CMAKE_MATCH_3})
  else()
    set(XRootD_VERSION_PATCH 0)
  endif()
  if(${CMAKE_MATCH_COUNT} GREATER 3)
    set(XRootD_VERSION_TWEAK ${CMAKE_MATCH_4})
  else()
    set(XRootD_VERSION_TWEAK 0)
  endif()
  math(EXPR XRootD_VERSION_NUMBER
    "10000 * ${XRootD_VERSION_MAJOR} + 100 * ${XRootD_VERSION_MINOR} + ${XRootD_VERSION_PATCH}"
    OUTPUT_FORMAT DECIMAL)
else()
  message(WARNING "Failed to determine XRootD version, using a timestamp as fallback."
    "You can override this by setting -DXRootD_VERSION_STRING=x.y.z during configuration.")
  set(XRootD_VERSION_MAJOR 5)
  set(XRootD_VERSION_MINOR 7)
  set(XRootD_VERSION_PATCH 0)
  set(XRootD_VERSION_TWEAK 0)
  set(XRootD_VERSION_NUMBER 1000000)
  string(TIMESTAMP XRootD_VERSION_STRING
    "v${XRootD_VERSION_MAJOR}.${XRootD_VERSION_MINOR}-rc%Y%m%d" UTC)
endif()

if(XRootD_VERSION_STRING MATCHES "[_-](.*)$")
  set(XRootD_VERSION_SUFFIX ${CMAKE_MATCH_1})
endif()

string(REGEX MATCH "[0-9]+[.]*[0-9]*[.]*[0-9]*[.]*[0-9]*(-rc)?[0-9].*"
  XRootD_VERSION ${XRootD_VERSION_STRING})

message(DEBUG "XRootD_VERSION_STRING = '${XRootD_VERSION_STRING}'")
message(DEBUG "XRootD_VERSION_NUMBER = '${XRootD_VERSION_NUMBER}'")
message(DEBUG "XRootD_VERSION_MAJOR  = '${XRootD_VERSION_MAJOR}'")
message(DEBUG "XRootD_VERSION_MINOR  = '${XRootD_VERSION_MINOR}'")
message(DEBUG "XRootD_VERSION_PATCH  = '${XRootD_VERSION_PATCH}'")
message(DEBUG "XRootD_VERSION_TWEAK  = '${XRootD_VERSION_TWEAK}'")
message(DEBUG "XRootD_VERSION_SUFFIX = '${XRootD_VERSION_SUFFIX}'")
message(DEBUG "XRootD_VERSION        = '${XRootD_VERSION}'")
