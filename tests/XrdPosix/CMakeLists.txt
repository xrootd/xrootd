add_executable(xrdposix-cat cat.cc)
target_link_libraries(xrdposix-cat XrdPosixPreload)

add_executable(xrdposix-statx statx.cc)
target_link_libraries(xrdposix-statx XrdPosixPreload)
