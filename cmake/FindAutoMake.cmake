FIND_PROGRAM( AUTOMAKE_FOUND automake
              HINTS
              ${AUTOMAKE_DIR}
              /usr
              PATH_SUFFIXES bin
)

INCLUDE( FindPackageHandleStandardArgs )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( AutoMake DEFAULT_MSG AUTOMAKE_FOUND )
