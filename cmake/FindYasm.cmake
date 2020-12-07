FIND_PROGRAM( YASM_FOUND yasm
              HINTS
              ${YASM_DIR}
              /usr
              PATH_SUFFIXES bin
)

INCLUDE( FindPackageHandleStandardArgs )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( Yasm DEFAULT_MSG YASM_FOUND )
