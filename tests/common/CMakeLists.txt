add_library(XrdTestUtils SHARED
  Utils.cc   Utils.hh)

target_link_libraries(XrdTestUtils ZLIB::ZLIB XrdCl XrdUtils)

target_include_directories(XrdTestUtils
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
