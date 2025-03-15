#-------------------------------------------------------------------------------
# Define the default build parameters
#-------------------------------------------------------------------------------
include( CMakeDependentOption )

option( ENABLE_CEPH      "Enable XrdCeph plugins."                                        FALSE )
option( ENABLE_FUSE      "Enable the fuse filesystem driver if possible."                 TRUE )
option( ENABLE_KRB5      "Enable the Kerberos 5 authentication if possible."              TRUE )
option( ENABLE_READLINE  "Enable the lib readline support in the commandline utilities."  TRUE )
option( ENABLE_XRDCL     "Enable XRootD client."                                          TRUE )
option( ENABLE_TESTS     "Enable unit tests."                                             FALSE )
cmake_dependent_option( ENABLE_SERVER_TESTS "Enable server tests." TRUE "ENABLE_TESTS" FALSE )
option( ENABLE_HTTP      "Enable HTTP component."                                         TRUE )
option( ENABLE_PYTHON    "Enable python bindings."                                        TRUE )
option( XRDCL_ONLY       "Build only the client and necessary dependencies"               FALSE )
option( XRDCL_LIB_ONLY   "Build only the client libraries and necessary dependencies"     FALSE )
option( PYPI_BUILD       "The project is being built for PyPI release"                    FALSE )
option( ENABLE_VOMS      "Enable VOMS plug-in if possible."                               TRUE )
option( ENABLE_XRDEC     "Enable erasure coding component."                               TRUE )
option( ENABLE_ASAN      "Enable adress sanitizer."                                       FALSE )
option( ENABLE_TSAN      "Enable thread sanitizer."                                       FALSE )
option( ENABLE_XRDCLHTTP "Enable xrdcl-http plugin."                                      TRUE )
cmake_dependent_option( ENABLE_SCITOKENS "Enable SciTokens plugin." TRUE "NOT XRDCL_ONLY" FALSE )
cmake_dependent_option( ENABLE_MACAROONS "Enable Macaroons plugin." TRUE "NOT XRDCL_ONLY" FALSE )
option( FORCE_ENABLED    "Fail build if enabled components cannot be built."              FALSE )

# backward compatibility
if(XRDCEPH_SUBMODULE)
  set(ENABLE_CEPH TRUE)
endif()

execute_process(COMMAND id -u OUTPUT_VARIABLE UID OUTPUT_STRIP_TRAILING_WHITESPACE)

if(XRDCL_ONLY OR XRDCL_LIB_ONLY OR UID EQUAL 0)
  set(ENABLE_SERVER_TESTS FALSE CACHE BOOL "Server not available or running as root" FORCE)
endif()
