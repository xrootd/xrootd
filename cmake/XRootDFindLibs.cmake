#-------------------------------------------------------------------------------
# Find the required libraries
#-------------------------------------------------------------------------------
find_package( ZLIB REQUIRED)

find_package( libuuid REQUIRED )

if( ENABLE_READLINE )
  if( FORCE_ENABLED )
    find_package( Readline REQUIRED )
  else()
    find_package( Readline )
  endif()
  if( READLINE_FOUND )
    add_definitions( -DHAVE_READLINE )
  else()
    set( READLINE_LIBRARY "" )
    set( NCURSES_LIBRARY "" )
  endif()
endif()

if( ZLIB_FOUND )
  add_definitions( -DHAVE_LIBZ )
endif()

find_package( TinyXml )

find_package( LibXml2 )
if( LIBXML2_FOUND )
  add_definitions( -DHAVE_XML2 )
endif()

find_package( systemd )
if( SYSTEMD_FOUND )
  add_definitions( -DHAVE_SYSTEMD )
endif()

find_package( CURL )

find_package( OpenSSL 1.0.2 REQUIRED )
add_definitions( -DHAVE_DH_PADDED )
add_definitions( -DHAVE_XRDCRYPTO )
add_definitions( -DHAVE_SSL )

if( ENABLE_KRB5 )
  if( FORCE_ENABLED )
    find_package( Kerberos5 REQUIRED )
  else()
    find_package( Kerberos5 )
  endif()
  if( KERBEROS5_FOUND )
    set( BUILD_KRB5 TRUE )
  else()
    set( BUILD_KRB5 FALSE )
  endif()
endif()

# mac fuse not supported
if( ENABLE_FUSE AND (LINUX OR KFREEBSD) )
  if( FORCE_ENABLED )
    find_package( fuse REQUIRED )
  else()
    find_package( fuse )
  endif()
  if( FUSE_FOUND )
    add_definitions( -DHAVE_FUSE )
    set( BUILD_FUSE TRUE )
  else()
    set( BUILD_FUSE FALSE )
  endif()
endif()

if( ENABLE_TESTS )
  if( FORCE_ENABLED )
    find_package( CppUnit REQUIRED )
    find_package( GTest REQUIRED )
  else()
    find_package( CppUnit )
    find_package( GTest )
  endif()
  if( CPPUNIT_FOUND AND GTEST_FOUND )
    set( BUILD_TESTS TRUE )
  else()
    set( BUILD_TESTS FALSE )
  endif()
endif()

if( ENABLE_HTTP )
  set( BUILD_HTTP TRUE )
  if( CURL_FOUND )
    set( BUILD_TPC TRUE )
  else()
    if( FORCE_ENABLED )
      message( FATAL_ERROR "Cannot build HttpTpc: missing CURL." )
    endif()
    set( BUILD_TPC FALSE )
  endif()
endif()

if( BUILD_TPC )
set ( CMAKE_REQUIRED_LIBRARIES ${CURL_LIBRARIES} )
check_function_exists( curl_multi_wait HAVE_CURL_MULTI_WAIT )
compiler_define_if_found( HAVE_CURL_MULTI_WAIT HAVE_CURL_MULTI_WAIT )
endif()

if( ENABLE_MACAROONS )
  if( FORCE_ENABLED )
    find_package( Macaroons REQUIRED )
  else()
    find_package( Macaroons )
  endif()
  if( NOT MacOSX )
    include(FindPkgConfig REQUIRED)
    if( FORCE_ENABLED )
      pkg_check_modules(JSON REQUIRED json-c)
      pkg_check_modules(UUID REQUIRED uuid)
    else()
      pkg_check_modules(JSON json-c)
      pkg_check_modules(UUID uuid)
    endif()
  endif()
  if( MACAROONS_FOUND AND JSON_FOUND AND UUID_FOUND )
    set( BUILD_MACAROONS TRUE )
  else()
    set( BUILD_MACAROONS FALSE )
  endif()
endif()


if( ENABLE_SCITOKENS )
  if( FORCE_ENABLED )
    find_package( SciTokensCpp REQUIRED )
  else()
    find_package( SciTokensCpp )
  endif()
  if( SCITOKENSCPP_FOUND )
    set( BUILD_SCITOKENS TRUE )
  else()
    set( BUILD_SCITOKENS FALSE )
  endif()
endif()

if( ENABLE_XRDEC )
  if( USE_SYSTEM_ISAL )
    if( FORCE_ENABLED )
      find_package(isal REQUIRED)
    else()
      find_package(isal)
    endif()
    if( ISAL_FOUND )
      set(BUILD_XRDEC TRUE)
    else()
      set(BUILD_XRDEC FALSE)
    endif()
  else()
    if( FORCE_ENABLED )
      find_package( Yasm REQUIRED )
      find_package( LibTool REQUIRED )
      find_package( AutoMake REQUIRED )
      find_package( AutoConf REQUIRED )
    else()
      find_package( Yasm )
      find_package( LibTool )
      find_package( AutoMake )
      find_package( AutoConf )
    endif()
    if( YASM_FOUND AND LIBTOOL_FOUND AND AUTOMAKE_FOUND AND AUTOCONF_FOUND )
      set( BUILD_XRDEC TRUE )
    else()
      set( BUILD_XRDEC FALSE )
    endif()
  endif()
endif()

if( ENABLE_PYTHON AND (LINUX OR KFREEBSD OR Hurd OR MacOSX) )
  if( FORCE_ENABLED )
    find_package( Python ${XRD_PYTHON_REQ_VERSION} COMPONENTS Interpreter Development REQUIRED )
  else()
    find_package( Python ${XRD_PYTHON_REQ_VERSION} COMPONENTS Interpreter Development )
  endif()
  if( Python_Interpreter_FOUND AND Python_Development_FOUND )
    set( BUILD_PYTHON TRUE )
  else()
    set( BUILD_PYTHON FALSE )
  endif()
endif()

if( ENABLE_VOMS AND (LINUX OR KFREEBSD OR Hurd) )
  if( FORCE_ENABLED )
    find_package( VOMS REQUIRED )
  else()
    find_package( VOMS )
  endif()
  if( VOMS_FOUND )
    set( BUILD_VOMS TRUE )
  else()
    set( BUILD_VOMS FALSE )
  endif()
endif()

if( ENABLE_XRDCLHTTP )
  if( FORCE_ENABLED )
    find_package( Davix REQUIRED )
  else()
    find_package( Davix )
  endif()
  if( DAVIX_FOUND )
    set( BUILD_XRDCLHTTP TRUE )
  else()
    set( BUILD_XRDCLHTTP FALSE )
  endif()
endif()
