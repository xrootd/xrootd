find_path(
    Davix_INCLUDE_DIRS
    NAMES davix/davix.hpp
    HINTS ${Davix_INCLUDE_DIRS}
)

find_library(
    Davix_LIBRARIES
    NAMES davix
    HINTS ${Davix_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
    Davix
    DEFAULT_MSG
    Davix_LIBRARIES
    Davix_INCLUDE_DIRS
)

if(Davix_FOUND)
    mark_as_advanced(Davix_LIBRARIES Davix_INCLUDE_DIRS)
endif()
