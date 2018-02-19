#-------------------------------------------------------------------------------
# Probe the system libraries
#-------------------------------------------------------------------------------

include( CheckFunctionExists )
include( CheckSymbolExists ) 
include( CheckLibraryExists )
include( CheckIncludeFile )
include( CheckCXXSourceRuns )
include( XRootDUtils )

#-------------------------------------------------------------------------------
# OS stuff
#-------------------------------------------------------------------------------
check_function_exists( setresuid HAVE_SETRESUID )
compiler_define_if_found( HAVE_SETRESUID HAVE_SETRESUID )

check_function_exists( strlcpy HAVE_STRLCPY )
compiler_define_if_found( HAVE_STRLCPY HAVE_STRLCPY )

check_function_exists( fstatat HAVE_FSTATAT )
compiler_define_if_found( HAVE_FSTATAT HAVE_FSTATAT )

check_function_exists( sigwaitinfo HAVE_SIGWTI )
compiler_define_if_found( HAVE_SIGWTI HAVE_SIGWTI )
if( NOT HAVE_SIGWTI )
  check_library_exists( rt sigwaitinfo "" HAVE_SIGWTI_IN_RT )
  compiler_define_if_found( HAVE_SIGWTI_IN_RT HAVE_SIGWTI )
endif()

check_include_file( shadow.h HAVE_SHADOWPW )
compiler_define_if_found( HAVE_SHADOWPW HAVE_SHADOWPW )

#-------------------------------------------------------------------------------
# Some socket related functions
#-------------------------------------------------------------------------------
check_function_exists( getifaddrs HAVE_GETIFADDRS )
compiler_define_if_found( HAVE_GETIFADDRS HAVE_GETIFADDRS )
check_function_exists( getnameinfo HAVE_NAMEINFO )
compiler_define_if_found( HAVE_NAMEINFO HAVE_NAMEINFO )
if( NOT HAVE_NAMEINFO )
  check_library_exists( socket getnameinfo "" HAVE_NAMEINFO_IN_SOCKET )
  compiler_define_if_found( HAVE_NAMEINFO_IN_SOCKET HAVE_NAMEINFO )
endif()

check_function_exists( getprotobyname_r HAVE_PROTOR )
compiler_define_if_found( HAVE_PROTOR HAVE_PROTOR )
if( NOT HAVE_PROTOR )
  check_library_exists( socket getprotobyname_r "" HAVE_PROTOR_IN_SOCKET )
  compiler_define_if_found( HAVE_PROTOR_IN_SOCKET HAVE_PROTOR )
endif()

check_function_exists( gethostbyaddr_r HAVE_GETHBYXR )
compiler_define_if_found( HAVE_GETHBYXR HAVE_GETHBYXR )
if( NOT HAVE_GETHBYXR )
  check_library_exists( socket gethostbyaddr_r "" HAVE_GETHBYXR_IN_SOCKET )
  compiler_define_if_found( HAVE_GETHBYXR_IN_SOCKET HAVE_GETHBYXR )
endif()

if( HAVE_GETHBYXR_IN_SOCKET OR HAVE_PROTOR_IN_SOCKET OR HAVE_NAMEINFO_IN_SOCKET )
  set( SOCKET_LIBRARY "-lsocket" )
else()
  set( SOCKET_LIBRARY "" )
endif()

#-------------------------------------------------------------------------------
# Sendfile
#-------------------------------------------------------------------------------
if( NOT MacOSX )
  check_function_exists( sendfile HAVE_SENDFILE )
  compiler_define_if_found( HAVE_SENDFILE HAVE_SENDFILE )
  set( SENDFILE_LIBRARY "" )
  if( NOT HAVE_SENDFILE )
    check_library_exists( sendfile sendfile "" HAVE_SENDFILE_IN_SENDFILE )
    compiler_define_if_found( HAVE_SENDFILE_IN_SENDFILE HAVE_SENDFILE )

    if( HAVE_SENDFILE_IN_SENDFILE )
      set( SENDFILE_LIBRARY "sendfile" )
    endif()
  endif()
endif()

#-------------------------------------------------------------------------------
# Check for libcrypt
#-------------------------------------------------------------------------------
check_function_exists( crypt HAVE_CRYPT )
compiler_define_if_found( HAVE_CRYPT HAVE_CRYPT )
if( NOT HAVE_CRYPT )
  check_library_exists( crypt crypt "" HAVE_CRYPT_IN_CRYPT )
  compiler_define_if_found( HAVE_CRYPT_IN_CRYPT HAVE_CRYPT )
  set( CRYPT_LIBRARY "-lcrypt" )
endif()
if( NOT HAVE_CRYPT AND NOT HAVE_CRYPT_IN_CRYPT )
  set( CRYPT_LIBRARY "" )
endif()

check_include_file( et/com_err.h HAVE_ET_COM_ERR_H )
compiler_define_if_found( HAVE_ET_COM_ERR_H HAVE_ET_COM_ERR_H )

#-------------------------------------------------------------------------------
# Check for the atomics
#-------------------------------------------------------------------------------
check_cxx_source_runs(
"
  int main()
  {
    unsigned long long val = 111, *mem = &val;

    if (__sync_fetch_and_add(&val, 111) != 111 || val != 222) return 1;
    if (__sync_add_and_fetch(&val, 111) != 333)               return 1;
    if (__sync_sub_and_fetch(&val, 111) != 222)               return 1;
    if (__sync_fetch_and_sub(&val, 111) != 222 || val != 111) return 1;

    if (__sync_fetch_and_or (&val, 0)   != 111 || val != 111) return 1;
    if (__sync_fetch_and_and(&val, 0)   != 111 || val != 0  ) return 1;

    if (__sync_bool_compare_and_swap(mem, 0, 444) == 0 || val != 444)
      return 1;

    return 0;
  }
"
HAVE_ATOMICS )
option(EnableAtomicsIfPresent "EnableAtomicsIfPresent" ON)
if ( EnableAtomicsIfPresent )
  compiler_define_if_found( HAVE_ATOMICS HAVE_ATOMICS )
endif ()



