

file(GLOB cryptlite_Sources   Kernel/cryptlite/*.h Kernel/cryptlite/*.c)
file(GLOB tinyxml_Sources     Kernel/tinyxml/*.h Kernel/tinyxml/*.cpp)
file(GLOB KernelSources       Kernel/*.h Kernel/*.cpp)
file(GLOB Sources             *.h *.cpp README.md)

source_group("Kernel\\cryptlite"    FILES ${cryptlite_Sources})
source_group("Kernel\\tinyxml"      FILES ${tinyxml_Sources})
source_group("Kernel"               FILES ${KernelSources})
source_group(""                     FILES ${Sources})
 
# no warning for now
add_compile_options("-w")

add_library(XrdObjectStorageOss  SHARED ${Sources} ${KernelSources} ${cryptlite_Sources} ${tinyxml_Sources} )

target_include_directories(XrdObjectStorageOss PUBLIC 
	${CMAKE_SOURCE_DIR}/src 
	${CMAKE_BINARY_DIR}/src 
	${CMAKE_CURRENT_SOURCE_DIR} 
	${CMAKE_CURRENT_SOURCE_DIR}/Kernel)

target_compile_options(XrdObjectStorageOss PUBLIC -D_FILE_OFFSET_BITS=64)
target_compile_options(XrdObjectStorageOss PUBLIC -D_LARGEFILE64_SOURCE=1)
target_compile_options(XrdObjectStorageOss PUBLIC -D_LARGEFILE_SOURCE=1)
target_compile_options(XrdObjectStorageOss PUBLIC -Wno-attributes -Wno-deprecated-declarations -Wno-error=sign-compare)	

target_link_libraries(XrdObjectStorageOss  PUBLIC curl dl rt util pthread)

add_executable(XrdObjectStorageOssTest  main.cpp)
target_link_libraries(XrdObjectStorageOssTest  PUBLIC XrdObjectStorageOss XrdServer)

set_target_properties(XrdObjectStorageOss     PROPERTIES FOLDER "")
set_target_properties(XrdObjectStorageOssTest PROPERTIES FOLDER "")