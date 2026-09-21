cmake_minimum_required(VERSION 3.16)
project(PrepareInstalledConsumer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
find_path(PREP_INCLUDE XrdOfs/XrdOfsPrepPersist.hh
  PATHS "${XROOTD_PREFIX}/include/xrootd" NO_DEFAULT_PATH REQUIRED)
find_library(PREP_LIBRARY XrdOfsPrepPersist
  PATHS "${XROOTD_PREFIX}/lib" "${XROOTD_PREFIX}/lib64" NO_DEFAULT_PATH REQUIRED)
find_library(UTILS_LIBRARY XrdUtils
  PATHS "${XROOTD_PREFIX}/lib" "${XROOTD_PREFIX}/lib64" NO_DEFAULT_PATH REQUIRED)
add_executable(prepare-consumer main.cc)
target_include_directories(prepare-consumer PRIVATE "${PREP_INCLUDE}")
target_link_libraries(prepare-consumer PRIVATE "${PREP_LIBRARY}" "${UTILS_LIBRARY}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
  target_link_libraries(prepare-consumer PRIVATE stdc++fs)
endif()
enable_testing()
add_test(NAME installed-prepare-consumer COMMAND prepare-consumer)
