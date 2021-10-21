
file(GLOB SOURCES *.cpp *.h) 

add_library( XrdObjectStorageOss SHARED ${SOURCES} README.md)

target_include_directories(XrdObjectStorageOss PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR}/src)
target_compile_definitions(XrdObjectStorageOss PRIVATE -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 )


