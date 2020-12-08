FIND_PROGRAM( AUTOCONF_FOUND autoconf
              HINTS
              ${AUTOCONF_DIR}
              /usr
              PATH_SUFFIXES bin
)

INCLUDE( FindPackageHandleStandardArgs )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( AutoConf DEFAULT_MSG AUTOCONF_FOUND )
