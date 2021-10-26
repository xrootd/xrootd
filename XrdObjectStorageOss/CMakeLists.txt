CMAKE_POLICY(SET CMP0079 NEW)

add_compile_options("-w")

# ///////////////////////////////////////////////////////////
# rsync -v -r ../../OpenVisus/Libs/Kernel/ ./Kernel/
# rsync -v -r ./Kernel/ ../../OpenVisus/Libs/Kernel/ 
SET(BUILD_SHARED_LIBS  OFF CACHE BOOL "disabled" FORCE)
set(VISUS_IMAGE        0)
set(VISUS_NET          0)
macro(DisableWarning Sources)
	if (MSVC)
		set_source_files_properties(${Sources} PROPERTIES COMPILE_FLAGS "/W0")
	else()
		set_source_files_properties(${Sources} PROPERTIES COMPILE_FLAGS "-w")
	endif()
endmacro()
add_subdirectory(Kernel)
target_compile_options(VisusKernel PRIVATE -DVISUS_NET=1 -fPIC)
target_link_libraries(VisusKernel PRIVATE curl)


source_group("" FILES  XrdObjectStorageOss.cpp XrdObjectStorageOss.h)
add_library(XrdObjectStorageOss  SHARED XrdObjectStorageOss.cpp XrdObjectStorageOss.h README.md)
set_target_properties(XrdObjectStorageOss PROPERTIES FOLDER "")
 
target_include_directories(XrdObjectStorageOss PUBLIC 
	${CMAKE_SOURCE_DIR}/src 
	${CMAKE_BINARY_DIR}/src 
	${CMAKE_CURRENT_SOURCE_DIR} 
	${CMAKE_CURRENT_SOURCE_DIR}/Kernel/include)

target_link_libraries(XrdObjectStorageOss PUBLIC VisusKernel)
target_compile_options(XrdObjectStorageOss PUBLIC -D_LARGEFILE_SOURCE=1)
target_compile_options(XrdObjectStorageOss PUBLIC -Wno-attributes -Wno-deprecated-declarations -Wno-error=sign-compare)	

