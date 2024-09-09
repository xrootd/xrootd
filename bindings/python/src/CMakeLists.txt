# WITH_SOABI was introduced in CMake 3.17
# https://cmake.org/cmake/help/latest/module/FindPython.html#commands
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.17)
  set(SOABI WITH_SOABI)
endif()

Python_add_library(client MODULE ${SOABI}
  # headers
  AsyncResponseHandler.hh
  ChunkIterator.hh
  Conversions.hh
  PyXRootD.hh
  PyXRootDCopyProcess.hh
  PyXRootDCopyProgressHandler.hh
  PyXRootDEnv.hh
  PyXRootDFile.hh
  PyXRootDFileSystem.hh
  PyXRootDFinalize.hh
  PyXRootDURL.hh
  Utils.hh
  # sources
  PyXRootDCopyProcess.cc
  PyXRootDCopyProgressHandler.cc
  PyXRootDFile.cc
  PyXRootDFileSystem.cc
  PyXRootDModule.cc
  PyXRootDURL.cc
  Utils.cc
)

target_compile_options(client PRIVATE -w) # TODO: fix build warnings

if(APPLE)
  set(CMAKE_MACOSX_RPATH TRUE)
  set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
  set_target_properties(client PROPERTIES INSTALL_NAME_DIR "@rpath")
endif()

# Avoid a call to find_package(XRootD) in order to be able to override
# variables when building the module as part of a standard CMake build.

if(TARGET XrdCl)
  target_link_libraries(client PRIVATE XrdCl)
else()
  find_library(XRootD_CLIENT_LIBRARY NAMES XrdCl)

  if(NOT XRootD_CLIENT_LIBRARY)
    message(FATAL_ERROR "Could not find XRootD client library")
  endif()

  find_path(XRootD_INCLUDE_DIR XrdVersion.hh PATH_SUFFIXES include/xrootd)

  if(NOT XRootD_INCLUDE_DIR)
    message(FATAL_ERROR "Could not find XRootD client include directory")
  endif()

  # The client library makes use of private XRootD headers, so add the
  # extra include for it to allow building the Python bindings against
  # a pre-installed XRootD.

  target_link_libraries(client PRIVATE ${XRootD_CLIENT_LIBRARY})
  target_include_directories(client PRIVATE ${XRootD_INCLUDE_DIR} ${XRootD_INCLUDE_DIR}/private)
endif()
