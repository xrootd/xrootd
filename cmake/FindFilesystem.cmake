
# Ideas come from
#
#  https://gitlab.kitware.com/cmake/cmake/-/issues/17834
#
# Basically, upstream CMake claims the fact that a separate library is
# needed for std::filesystem support is a short-lived fact (of all the
# platforms we use, only RHEL 8 uses a compiler where this is needed),
# hence they don't want a standardized way to detect std::filesystem

include(CheckSourceCompiles)

set( SAMPLE_FILESYSTEM "#include <cstdlib>
        #include <filesystem>

        int main() {
            auto cwd = std::filesystem::current_path();
            return cwd.empty();
        }")


CHECK_SOURCE_COMPILES( CXX "${SAMPLE_FILESYSTEM}" CXX_FILESYSTEM_NO_LINK_NEEDED )

set( _found FALSE )
if( CXX_FILESYSTEM_NO_LINK_NEEDED )
  set( _found TRUE )
else()
  # Add the libstdc++ flag; while this is a GCC-specific library name,
  # all supported versions of clang do not take this branch.
  set( CMAKE_REQUIRED_LIBRARIES "-lstdc++fs" )
  CHECK_SOURCE_COMPILES( CXX "${SAMPLE_FILESYSTEM}" CXX_FILESYSTEM_STDCPPFS_NEEDED )
  set( _found TRUE )
endif()

add_library( std::filesystem INTERFACE IMPORTED )
#set_property( TARGET std::filesystem APPEND PROPERTY INTERFACE_COMPILE_FEATURES cxx_std_17 )

if( CXX_FILESYSTEM_STDCPPFS_NEEDED )
  set_property( TARGET std::filesystem APPEND PROPERTY INTERFACE_LINK_LIBRARIES -lstdc++fs )
endif()

set( Filesystem_FOUND ${_found} CACHE BOOL "TRUE if we can run a program using std::filesystem" FORCE )
if( Filesystem_FIND_REQUIRED AND NOT Filesystem_FOUND )
    message( FATAL_ERROR "Cannot run simple program using std::filesystem" )
endif()
