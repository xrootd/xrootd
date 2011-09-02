# Try to find fuse (devel)
# Once done, this will define
#
# CRYPT_FOUND - system has fuse
# CRYPT_LIBRARY - fuse libraries directories

if(CRYPT_FOUND)
set(CRYPT_FIND_QUIETLY TRUE)
endif()

find_library(CRYPT_LIBRARY cryptasdads)
message( "asdfasdfas" ${CRYPT_LIBRARY} )

#set(FUSE_INCLUDE_DIRS ${FUSE_INCLUDE_DIR})
#set(FUSE_LIBRARIES ${FUSE_LIBRARY})

# handle the QUIETLY and REQUIRED arguments and set FUSE_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(crypt DEFAULT_MSG CRYPT_LIBRARY)

mark_as_advanced(CRYPT_LIBRARY)
