FIND_PROGRAM( LIBTOOL_FOUND libtool
              HINTS
              ${LIBTOOL_DIR}
              /usr
              PATH_SUFFIXES bin
)

INCLUDE( FindPackageHandleStandardArgs )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( LibTool DEFAULT_MSG LIBTOOL_FOUND )
