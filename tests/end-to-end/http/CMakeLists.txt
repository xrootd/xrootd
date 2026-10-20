add_subdirectory(header)

if(BUILD_HTTP)
  add_subdirectory(put)
endif()

if(BUILD_HTTP AND TARGET XrdClHttpObj AND TARGET xrootd AND TARGET xrdfs)
  add_subdirectory(worker)
endif()
