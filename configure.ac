#######################################################################
#                      configure.ac
# Process this file with autoconf to produce a configure script.
#######################################################################
##
## (c) 2004 by the Board of Trustees of the Leland Stanford, Jr., University
##       All Rights Reserved. See XrdInfo.cc for complete License Terms
##   Produced by Andrew Hanushevsky for Stanford University under contract
##              DE-AC03-76-SFO0515 with the Department of Energy
##
## configure.ac autotools configuration file for xrootd
## Author: Derek Feichtinger <derek.feichtinger@cern.ch>
## Initial version: 1.8.2005
##
## Version info: $Id$
## Checked in by $Author$
#######################################################################

AC_PREREQ(2.57)
AC_INIT(xrootd,20051004.1010x4,xrootd-l@slac.stanford.edu)
AC_CONFIG_SRCDIR([src/XrdVersion.hh])

###############################################################
# Set some defaults which will be augmented by the OS detection
# routine below

# extra directories for standard search path for header files
extra_inc_path_default=" "

# extra directories for standard search path for libraries
extra_lib_path_default=" "

# preference for compilers
CC_PREFERENCE="gcc cc"
CXX_PREFERENCE="g++ c++"

# Note:
# adding wildly to CFLAGS, CXXFLAGS, LDFLAGS, etc is not good
# style, because the user should have the possibility to use
# the provided --with-FEATURE-DIR options to specify locations.
# The separately specified LDFLAGS (like -Lpath) will be put
# in front of the usual options and override them!

###############################################################

# detect system type and define some specific settings
AC_CANONICAL_TARGET
case "$target_os" in
   tiger*)
     AC_DEFINE(__macos__)
     ;;
   darwin*)
     AC_DEFINE(__macos__)
     ;;
   apple*)
     AC_DEFINE(__macos__)
     ;;
   linux*)
     # TODO: check why two different macros are used for this
     AC_DEFINE(__linux__)
     ;;
   solaris*)
     TARGET_OS=solaris
     AC_DEFINE(__solaris__)
     CC_PREFERENCE="cc gcc"
     CXX_PREFERENCE="CC g++"
     DFLT_OPT="-O2"
     extra_inc_path_default="$extra_inc_path_default /usr/local/include"
     extra_lib_path_default="$extra_lib_path_default /usr/local/lib"
     ;;
   *)
     AC_MSG_WARN([untested operating system])
   ;;
esac

case "$target_cpu" in
   i386)
     AC_DEFINE(__i386__)
     ;;
   *)
     ;;
esac

###########################################################
# At SLAC the Kerberos libs and includes are under AFS; 
# people there want some automatic tool to pick them up, but
# having the paths hardcoded in the search lists may hang
# configure in other places if access to SLAC AFS is down.
# So we include them only for nodes in the domain '.stanford.edu'.
domain=""
case "$target_os" in
   linux*)
     domain=`dnsdomainname 2> /dev/null`
     ;;
   *)
     domain=`domainname 2> /dev/null`
     ;;
esac
if test "x$domain" = "x" ; then
   domain=`domainname 2> /dev/null`
fi
extra_inc_path_default="$extra_inc_path_default /usr/kerberos/include"
extra_lib_path_default="$extra_lib_path_default /usr/kerberos/lib"
case $domain in
  *.stanford.edu)
     extra_inc_path_default="$extra_inc_path_default /afs/slac.stanford.edu/package/krb5/include "
     extra_lib_path_default="$extra_lib_path_default /afs/slac.stanford.edu/package/krb5/lib"
     ;;
  *)
     ;;
esac

###########################################################
# only carry on the paths which really exist on this system
for name in $extra_inc_path_default ; do
   if test -d "$name"; then
      extra_inc_path="$extra_inc_path $name"
   fi
done
for name in $extra_lib_path_default ; do
   if test -d "$name"; then
      extra_lib_path="$extra_lib_path $name"
   fi
done

ACX_WITH_BASEDIR
extra_inc_path="$acx_base_incdir $extra_inc_path"
extra_lib_path="$acx_base_libdir $extra_lib_path"


############################################################
# set some variables according to what has been found so far

# for some tests we need the standard include path.
# We need to add any settings the user may have set via CPPFLAGS
for opt in $CPPFLAGS ; do
   case $opt in
      -I*)
         dir=`expr x"$opt" : 'x-I\(.*\)'`
         std_inc_path="$std_inc_path $dir"
         ;;
      *)
        ;;
   esac
done
std_inc_path="$std_inc_path $extra_inc_path /usr/include"

# add extra include path elements to CPPFLAGS
for dir in $extra_inc_path ; do
   if test x"$dir" != x ; then
      CPPFLAGS="$CPPFLAGS -I$dir"
   fi
done


# add extra library path elements to LDFLAGS
for dir in $extra_lib_path ; do
   if test x"$dir" != x ; then
      LDFLAGS="$LDFLAGS -L$dir"
   fi
done

AC_MSG_NOTICE([my CPPFLAGS... $CPPFLAGS])
AC_MSG_NOTICE([my LDFLAGS... $LDFLAGS])

############################################################


AC_ARG_ENABLE(dbg-build,
              [  --enable-dbg-build      build with debug symbols, no optimization],
  if test x"$enableval" = "xyes"; then
     debug_build=yes
     DFLT_OPT=""
     DBG_OPT="-g"
     AC_MSG_NOTICE([ENABLING DEBUG BUILD])
  fi
)


#################################################################
# Setting optimization and debug compiler options
#    Note: needs to be set before calling AC_PROG_CXX, because
#    if CXXFLAGS is empty, that macro will fill it with the
#    autoconf default (which is -g for any non-gcc compiler)

if test x"$DBG_OPT""$DFLT_OPT" != x ; then
   CXXFLAGS="$DBG_OPT $DFLT_OPT $CXXFLAGS"
fi

# get compiler information
AC_PROG_CC([$CC_PREFERENCE])
AC_PROG_CXX([$CXX_PREFERENCE])
AM_CONDITIONAL(IS_GCC,[test x"$GCC" = xyes])

#   Note: necessary for older distributions (e.g. RH7.3): sets a number of C standards macros
if test x"$GCC" = xyes ; then
   AC_GNU_SOURCE
fi

AM_INIT_AUTOMAKE(1.6 subdir-objects)

# we save the environment's value for CPPFLAGS,etc 
# because we will modify them for some of the tests
CPPFLAGS_BUP="$CPPFLAGS"
LDFLAGS_BUP="$LDFLAGS"

###################################################################
# OPTIONAL PACKAGES AND DIRECTIVES
###################################################################


####################################################################
# passwd based authentication support
AC_ARG_ENABLE(pwd,
              [  --enable-pwd            Password-based authentication support [[default=yes]] ],
              [],[enable_pwd=yes])
AM_CONDITIONAL(ENABLE_PWD, test x"$enable_pwd" = xyes)
if test x"$enable_pwd" = xyes ; then
   if test -f "/etc/shadow"; then
     AC_DEFINE(R__SHADOWPW)
   fi
   AC_CHECK_LIB(crypt,[crypt],
                 [AC_SUBST(CRYPTLIB,[-lcrypt])],
		 [AC_DEFINE(DONT_HAVE_CRYPT)
		  AC_MSG_WARN([disabling support for crypt-like passwords])
		 ],[])
fi

####################################################################
# Kerberos IV support
AC_ARG_ENABLE(krb4,
              [  --enable-krb4           Kerberos4 support, requires Kerberos libs [[default=no]] ],
              [],[enable_krb4=no]
             )
AC_ARG_WITH(krb4-incdir,
            [  --with-krb4-incdir=PATH     KerberosIV support, location of kerberosIV/krb.h],
            [if test ! -f "$withval"/kerberosIV/krb.h ; then
                AC_MSG_ERROR([Could not locate kerberosIV/krb.h in $withval])
             fi
	    KRB4_INCDIR="-I$withval"
            ]
           )
AC_SUBST(KRB4_INCDIR)

AC_ARG_WITH(krb4-libdir,
            [  --with-krb4-libdir=PATH     KerberosIV support, location of libkrb4],
            [KRB4_LIBDIR="-L$withval"]
           )
AC_SUBST(KRB4_LIBDIR)



####################################################################
# Kerberos 5 support
AC_ARG_ENABLE(krb5,
              [  --enable-krb5           Kerberos5 support, requires Kerberos libs [[default=no]] ],
              [],[enable_krb5=no]
             )
AC_ARG_WITH(krb5-incdir,
            [  --with-krb5-incdir=PATH     Kerberos5 support, location of krb5.h],
            [if test ! -f "$withval"/krb5.h ; then
                AC_MSG_ERROR([Could not locate krb5.h in $withval])
             fi]
	    KRB5_INCDIR="-I$withval"
           )
AC_SUBST(KRB5_INCDIR)

AC_ARG_WITH(krb5-libdir,
            [  --with-krb5-libdir=PATH     Kerberos5 support, location of libkrb5],
            [KRB5_LIBDIR="-L$withval"]
           )
AC_SUBST(KRB5_LIBDIR)


####################################################################
# GSI support
AC_ARG_ENABLE(gsi,
              [  --enable-gsi     GSI authentication support, requires OpenSSL [[default=no]] ],
              [],[enable_gsi=no]
              )
if test x"$enable_gsi" = xyes ; then
   enable_openssl=yes
fi

AM_CONDITIONAL(ENABLE_GSI,test x"$enable_gsi" = xyes)


####################################################################
# OpenSSL support
AC_ARG_ENABLE(openssl,
              [  --enable-openssl       Build OpenSSL implementation of crypto interfaces [[default=yes]] ],
              [],[enable_openssl=default_yes]
             )

if test x"$enable_openssl" != xno ; then
  ssltests=ok
  OPENSSLINC=""
  AC_ARG_WITH(openssl-incdir,
            [  --with-openssl-incdir=PATH   dir containing openssl/ header directory],
            [searchpath="$withval"],
            [searchpath="$std_inc_path"]
           )

  ACX_LOCATEFILE([openssl/opensslv.h],[$searchpath],
                 [OPENSSL_INCDIR="-I$acx_founddir"
                  OPENSSL_VERSION_TEXT=`grep OPENSSL_VERSION_TEXT "$dir"/openssl/opensslv.h | grep OpenSSL`
                  AC_MSG_NOTICE([openssl header dir set to: $dir ]) ],
                 [if test  x"$enable_openssl" = xyes ; then
                    AC_MSG_ERROR([Could not locate openssl/opensslv.h in $searchpath])
                  fi]
                )

  AC_SUBST(OPENSSL_INCDIR)

  # test openssl version
  verstr=`expr  x"$OPENSSL_VERSION_TEXT" : '.*\([[0-9]][[0-9]]*\.[[0-9]][[0-9]]*\.[[0-9]][[0-9]]*[[a-z]]*\)'`
  AC_MSG_NOTICE([OpenSSL version : $verstr])

  vermajor=`expr x"$verstr" : '.*\([[0-9]][[0-9]]*\).[[0-9]][[0-9]]*.[[0-9]][[0-9]]*'`
  verminor=`expr x"$verstr" : '.*[[0-9]][[0-9]]*\.\([[0-9]][[0-9]]*\)\.[[0-9]][[0-9]]*'`
  verrelease=`expr x"$verstr" : '.*[[0-9]][[0-9]]*\.[[0-9]][[0-9]]*\.\([[0-9]][[0-9]]*\)'`
  vernum=`expr 10000 \* 0"$vermajor" + 100 \* 0"$verminor" + 0"$verrelease"`
  if test 0"$vernum" -lt 906 ; then
    AC_MSG_WARN([OpenSSL >= 0.9.6 required (found: $OPENSSL_VERSION_TEXT)
                ssltests=failed])
  fi
  #AC_MSG_NOTICE([Openssl version: $vermajor  $verminor  $verrelease == $vernum])

  # note: -DPERL5 is needed for resolving a clash between unistd.h and
  # openssl/des.h regarding the crypt function on some older systems (e.g. RH7.3)
  if test 0"$vernum" -lt 907 ; then
    AC_DEFINE(R__SSL_096)
    AC_SUBST(SSLCXXFLAGS,[-DPERL5])
  fi

  # note: -DR__SSL_GE_098 allows to deal with some differences in ANS1 macros
  # introduced in version 0.9.8
  if test 0"$vernum" -ge 908 ; then
    AC_DEFINE(R__SSL_GE_098)
  fi

  AC_ARG_WITH(openssl-libdir,
            [  --with-openssl-libdir=PATH    dir containing openssl library (libssl.*)],
            [
              OPENSSL_LIBDIR="-L$withval";
              AC_MSG_NOTICE([openssl library dir set to: ${OPENSSL_LIBDIR}])
            ]
           )
  AC_SUBST(OPENSSL_LIBDIR)


  # note: openssl may require kerberos5 include files
  CPPFLAGS="$OPENSSL_INCDIR $KRB5_INCDIR $CPPFLAGS"
  AC_CHECK_HEADER([openssl/ssl.h],
                [],[ssltests=failed]
                )


  LDFLAGS="$OPENSSL_LIBDIR $LDFLAGS"
  AC_CHECK_LIB([ssl],[SSL_library_init],
               [:],[ssltests=failed],[-lcrypto]
              )

  if test x"$ssltests" != xok ; then
    if test x"$enable_openssl" = xyes ; then
      AC_MSG_ERROR([tests for openssl failed. Change configure options])
    fi
    AC_MSG_WARN([disabling openssl support])
    enable_openssl=no
  fi

  CPPFLAGS="$CPPFLAGS_BUP"
  LDFLAGS=$LDFLAGS_BUP
fi

AM_CONDITIONAL(ENABLE_OPENSSL,test x"$enable_openssl" != xno)

####################################################################
# monitoring
AC_ARG_ENABLE(mon,
              [  --enable-mon     Enable/disable building of monitoring services [[default=no]] ],
              [],[enable_mon=no]
             )
AM_CONDITIONAL(ENABLE_MON,test x"$enable_mon" = xyes)

####################################################################
# PERL options
AC_ARG_ENABLE(perlint,
              [  --enable-perlint      build perl interface [[default=yes]] ],
              [],[enable_perlint=yes]
             )
AM_CONDITIONAL(ENABLE_PERLINT, test x"$enable_perlint" = xyes)

if test x"$enable_perlint" = xyes ; then
  AC_ARG_WITH(perl,
            [  --with-perl=PATH        path to perl executable],
            PERL=$withval,
            PERL=perl)
  AC_SUBST(PERL)
  # get the correct library options and link flags for perl
  AC_MSG_CHECKING([perl libraries and link options])
  perlbuf=`${PERL} -MExtUtils::Embed -e ldopts`
  if test x"$perlbuf" = x
  then
    AC_MSG_ERROR([no result from ${PERL} -MExtUtils::Embed -e ldopts])
  fi
  AC_MSG_RESULT($perlbuf)
  for name in $perlbuf
  do
    case $name in
      -L*)
        PERL_LIBS="${PERL_LIBS} $name"
        ;;
      -l*)
        PERL_LIBS="${PERL_LIBS} $name"
        ;;
       *.a)
        PERL_LIBS="${PERL_LIBS} $name"
        ;;
      *)
        PERL_LDOPTS="${PERL_LDOPTS} $name"
        ;;
    esac
  done
  AC_MSG_NOTICE([perlembed libraries to link: $PERL_LIBS])
  AC_MSG_NOTICE([perlembed link flags: $PERL_LDOPTS])
  PERL_CCOPTS=`${PERL} -MExtUtils::Embed -e ccopts`
  AC_MSG_NOTICE([perlembed cc options: $PERL_CCOPTS])
  AC_SUBST(PERL_LIBS)
  AC_SUBST(PERL_LDOPTS)
  AC_SUBST(PERL_CCOPTS)
fi

#################################################################
# JAVA interface option
AC_ARG_ENABLE(javaint,
              [  --enable-javaint      build java interface [[default=no]] ],
              [],[enable_javaint=no])
AM_CONDITIONAL(ENABLE_JAVAINT, test x"$enable_javaint" = xyes)
MY_JAVA_HOME=$JAVA_HOME
JAVA_CLASSPATH="\$(libdir)"
AC_ARG_WITH(java-home,
	      [  --with-java-home=PATH  JAVA_HOME path],
              MY_JAVA_HOME=$withval)
AC_ARG_WITH(java-classpath,
	      [  --with-java-classpath=PATH  path where to install java .class files],
              JAVA_CLASSPATH=$withval)

if test x"$enable_javaint" = xyes; then
   if test ! -d "${MY_JAVA_HOME}/include"; then
      AC_MSG_ERROR([java include folder not found: ${MY_JAVA_HOME}/include.
   Disable javaint or use correct --with-java-home directive])
   fi
   JAVA_INCDIR="-I${MY_JAVA_HOME}/include -I${MY_JAVA_HOME}/include/linux"
   AC_MSG_NOTICE([java include settings: $JAVA_INCDIR])
   JAVA="$MY_JAVA_HOME"/bin/java
   JAVAC="$MY_JAVA_HOME"/bin/javac
   if test ! -x "$JAVA" ; then
      AC_MSG_ERROR([java executable not found at $JAVA])
   fi
   if test ! -x "$JAVAC" ; then
      AC_MSG_ERROR([java compiler not found at $JAVAC])
   fi
   javaverstr=`$JAVA -version 2>&1`
   javaverstr=`expr x"$javaverstr" : '.*\([[0-9]][[0-9]]*\.[[0-9]][[0-9]]*\.[[0-9]][[0-9]]*[[a-z]]*\)'`
   javavermajor=`expr x"$javaverstr" : '.*\([[0-9]][[0-9]]*\).[[0-9]][[0-9]]*.[[0-9]][[0-9]]*'`
   javaverminor=`expr x"$javaverstr" : '.*[[0-9]][[0-9]]*\.\([[0-9]][[0-9]]*\)\.[[0-9]][[0-9]]*'`
   javaverrelease=`expr x"$javaverstr" : '.*[[0-9]][[0-9]]*\.[[0-9]][[0-9]]*\.\([[0-9]][[0-9]]*[[a-z]]*\)'`
   javavernum=`expr 10000 \* 0"$javavermajor" + 100 \* 0"$javaverminor" + 0"$javaverrelease"`
   AC_MSG_NOTICE([java version is $javaverstr ($javavernum)])
   if test "$javavernum" -lt 10500; then
      AC_MSG_ERROR([java version > 1.5 needed])
   fi 
fi
AC_SUBST(JAVA_INCDIR)
AC_SUBST(JAVAC)
AC_SUBST(JAVA_CLASSPATH)
#################################################################
# Catalog token authorization mechanism
AC_ARG_ENABLE(tokenauthz,
        [  --enable-tokenauthz      enable building of catalog token authorization Ofs [[default=no]] ],
              [],[enable_tokenauthz=no]
             )

### we no longer invoke the sub-configure
#if test x"$enable_tokenauthz" = xyes ; then
#  AC_CONFIG_SUBDIRS(src/XrdTokenAuthOfs)
#fi

AM_CONDITIONAL(ENABLE_TOKENAUTHZ, test x"$enable_tokenauthz" = xyes)


AC_ARG_WITH(tkauthz-inc,
            [  --with-tkauthz-inc=PATH    directory containing TTokenAuthz.h],
            [TKAUTHZ_INC="-I${withval}"]
           )
AC_SUBST(TKAUTHZ_INC)
AC_ARG_WITH(tkauthz-lib,
            [  --with-tkauthz-lib=PATH    directory containing libTokenAuthz],
            [TKAUTHZ_LIB="-L${withval}"]
           )
AC_SUBST(TKAUTHZ_LIB)

# if we configure from this top level configure, we are always using the autotools build
AM_CONDITIONAL(XROOTD_AUTOBUILD, test 1 = 1)

# and therefore the srcdir for xrootd is "../.."
AC_SUBST(XROOTD_SRCDIR,[../..])

#################################################################
# where to install the utility scripts
AC_ARG_WITH(utils-dir,
          [  --with-utils-dir=PATH        install directory for utility scripts [[default=sysconfdir]] ],
          XROOTDUTILSDIR=$withval,
          XROOTDUTILSDIR="$bindir/xrootdutils")
AC_SUBST(XROOTDUTILSDIR)


#################################################################
# Checks for programs.

if test `expr x"$target_os" : 'xsolaris'` -gt 7 ; then
   if test -e /usr/xpg4/bin/egrep ; then
      AC_SUBST(EGREP,[/usr/xpg4/bin/egrep])
      AC_MSG_NOTICE([Solaris: using /usr/xpg4/bin/egrep for EGREP]) 
   fi
fi

dnl Disable libtool 1.5 support for languages we don't use (this is a HACK!)
define([AC_LIBTOOL_LANG_F77_CONFIG], [:])dnl
define([AC_LIBTOOL_LANG_GCJ_CONFIG], [:])dnl

AC_PROG_LIBTOOL

AC_PATH_PROGS(DOXYGEN,[doxygen],
               [no], [$PATH])
AC_SUBST(DOXYGEN)
AM_CONDITIONAL(HAVE_DOXYGEN,[test x"$DOXYGEN" != x])

#################################################################
# additional architecture dependent compiler flags

# this seems unnecessary and is often set in standard system includes
#AC_C_BIGENDIAN([AC_DEFINE(_BIG_ENDIAN)],
#               [AC_DEFINE(_LITTLE_ENDIAN)]
#              )

#  Note: This standard should already be included with the _GNU_SOURCE nacro
AC_DEFINE(_LARGEFILE_SOURCE)

case "$target_os" in
   solaris*)
      # note: this is for the SunCC compiler
      if test x"$GCC" != xyes ; then
         CXXFLAGS="-DSUNCC -D_POSIX_PTHREAD_SEMANTICS -xarch=v8plus"
         LDFLAGS="$LDFLAGS -mt"
         AC_SUBST(LIBSUNMATH,[-lsunmath])
      fi
      ;;
esac


#################################################################
# Checks for libraries.
AC_CHECK_LIB([pthread],[pthread_join],
             [LIBPTHREAD="-lpthread"])
AC_CHECK_LIB([pthreads],[pthread_join],
             [
             if test x"$LIBPTHREAD" != x; then
               AC_MSG_WARN([found both libpthread and libpthreads. Using -lpthread])
             else
               LIBPTHREAD="-lpthreads"
             fi
             ])
AC_SUBST(LIBPTHREAD)

AC_CHECK_LIB([rt],[aio_fsync],
             [AC_SUBST(LIBRT,[-lrt])])

##  ---- DISABLED FOR NOW ----
## check for the MonALISA client cpp library
##AC_CHECK_LIB([apmoncpp],[main],
##             [:],
##             [have_apmon=xno
##              AC_MSG_WARN([libapmoncpp not found. I Will not build xrdmoncp!])
##             ])
##AM_CONDITIONAL(ENABLE_APMON, test x"$have_apmon" != xno)

#################################################################
# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h limits.h malloc.h memory.h netdb.h netinet/in.h shadow.h stdlib.h string.h strings.h sys/file.h sys/mount.h sys/param.h sys/socket.h sys/statfs.h sys/statvfs.h sys/time.h sys/vfs.h unistd.h utime.h])


#################################################################
# Checks for typedefs, structures, and compiler characteristics.
#AC_HEADER_STDBOOL
#AC_C_CONST
#AC_TYPE_UID_T
#AC_C_INLINE
#AC_TYPE_MODE_T
#AC_TYPE_OFF_T
#AC_TYPE_PID_T
#AC_C_RESTRICT
#AC_TYPE_SIZE_T
#AC_CHECK_MEMBERS([struct stat.st_blksize])
#AC_STRUCT_ST_BLOCKS
#AC_HEADER_TIME
#AC_STRUCT_TM
#AC_C_VOLATILE
AM_PROG_CC_C_O

#################################################################
# Checks for library functions.
#AC_FUNC_CLOSEDIR_VOID
#AC_FUNC_ERROR_AT_LINE
#AC_FUNC_FORK
#AC_FUNC_LSTAT
#AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
#AC_FUNC_MALLOC
#AC_FUNC_MEMCMP
#AC_FUNC_MKTIME
#AC_FUNC_MMAP
#AC_FUNC_REALLOC
#AC_TYPE_SIGNAL
#AC_FUNC_STAT
#AC_FUNC_STRERROR_R
#AC_FUNC_UTIME_NULL
#AC_FUNC_VPRINTF
#AC_CHECK_FUNCS([bzero dup2 endgrent ftruncate getcwd gethostbyaddr gethostbyname gethostname getpagesize getpass getspnam gettimeofday inet_ntoa localtime_r memchr memmove memset mkdir munmap pathconf putenv rmdir socket strcasecmp strchr strcspn strdup strerror strncasecmp strspn strstr strtol utime])

AC_CHECK_FUNC([getnameinfo],[AC_DEFINE(HAS_NAMEINFO)],
              [AC_CHECK_LIB([socket],[getnameinfo],
                 [
                 AC_DEFINE(HAS_NAMEINFO)
                 AC_SUBST(LIBSOCKET,[-lsocket])
                 ])
              ])

AC_CHECK_FUNC([getprotobyname_r],[AC_DEFINE(HAS_PROTOR)],
              [AC_CHECK_LIB([socket],[getprotobyname_r],
                  [
                  AC_DEFINE(HAS_PROTOR)
                  AC_SUBST(LIBSOCKET,[-lsocket])
                  ])
              ])

#   note: needed for solaris
AC_CHECK_LIB([rt],[sem_post],
             [AC_SUBST(SEM_LIBRT,[-lrt])]
            )

#   note: needed for solaris (network services library. many systems still have these
#         these function still in the libc)
AC_CHECK_LIB([nsl],[gethostbyname],
             [AC_SUBST(LIBNSL,[-lnsl])]
            )

AC_CHECK_FUNC(sigwaitinfo,[AC_DEFINE(HAS_SIGWTI)],
                [AC_CHECK_LIB([rt],[sigwaitinfo],
                   [
                   AC_DEFINE(HAS_SIGWTI)
                   AC_SUBST(LIBRT,[-lrt])
                   ])
                ])

# On linux we need to check if setresuid prototype is defined
case "$target_os" in
   linux*)
      AC_CHECK_FUNCS(setresuid,
                     AC_DEFINE(HAS_SETRESUID)
                    )
      ;;
esac

###########################################
# Kerberos 4 checks
krb4tests=ok
CPPFLAGS="$KRB4_INCDIR $CPPFLAGS"
AC_CHECK_HEADER([kerberosIV/krb.h],
                [],[krb4tests=failed]
               )

LDFLAGS="$KRB4_LIBDIR $LDFLAGS"
AC_CHECK_LIB([krb4],[krb_get_lrealm],
             [:],[krb4tests=failed],[-ldes425 -lkrb5 -lk5crypto -lcom_err -lresolv  $LIBSOCKET  $LIBNSL]
            )

if test x"$krb4tests" = xfailed ; then
  if test x"$enable_krb4" = xyes ; then
    ACX_MSG_ERROR([tests for krb4 failed. Change configure options!])
  fi
  AC_MSG_WARN([tests for krb4 failed. Disabling krb4 support])
  enable_krb4=no
fi
CPPFLAGS="$CPPFLAGS_BUP"
LDFLAGS="$LDFLAGS_BUP"

AM_CONDITIONAL(ENABLE_KRB4,test x"$enable_krb4" = xyes)

###########################################
# Kerberos 5 checks
krb5tests=ok
CPPFLAGS="$KRB5_INCDIR $CPPFLAGS"
AC_CHECK_HEADER([krb5.h],
                [],[krb5tests=failed]
               )

LDFLAGS="$KRB5_LIBDIR $LDFLAGS"
AC_CHECK_LIB([krb5],[krb5_init_context],
             [:],[krb5tests=failed],[-lk5crypto -lcom_err]
            )

if test x"$krb5tests" = xfailed ; then
  if test x"$enable_krb5" = xyes ; then
    ACX_MSG_ERROR([tests for krb5 failed. Change configure options!])
  fi
  AC_MSG_WARN([tests for krb5 failed. Disabling krb5 support])
  enable_krb5=no
fi
LDFLAGS="$LDFLAGS_BUP"
CPPFLAGS="$CPPFLAGS_BUP"

AM_CONDITIONAL(ENABLE_KRB5,test x"$enable_krb5" = xyes)


CXXFLAGS="$CXXFLAGS -D_REENTRANT"

#################################################################


AC_OUTPUT(Makefile
          src/Makefile
          src/XrdSys/Makefile
          src/XrdOuc/Makefile
          src/XrdNet/Makefile
          src/XrdSec/Makefile
          src/XrdSut/Makefile
	  src/XrdCrypto/Makefile
          src/XrdSecpwd/Makefile
	  src/XrdSeckrb4/Makefile
	  src/XrdSeckrb5/Makefile
	  src/XrdSecgsi/Makefile
	  src/XrdSfs/Makefile
          src/XrdAcc/Makefile
          src/XrdOdc/Makefile
          src/XrdXr/Makefile
          src/XrdOss/Makefile
          src/XrdOfs/Makefile
          src/XrdOlb/Makefile
          src/Xrd/Makefile
          src/XrdRootd/Makefile
          src/XrdXrootd/Makefile
          src/XrdClient/Makefile
          src/XrdPosix/Makefile
          src/XrdMon/Makefile
          src/XrdTokenAuthzOfs/Makefile
          xrootd.spec)

#####################################################################
# we need to move the GNUMakefiles of the classic build away
if test -e src/XrdOuc/GNUmakefile ; then
   echo "###########################################################"
   echo "Moving away the old GNUmakefiles to GNUmakefile.classic"
   if test `expr x"$target_os" : 'xsolaris'` -gt 7 ; then
      find . -name GNUmakefile | xargs -i mv {} {}.classic 
   else
      find . -name GNUmakefile -exec mv {} {}.classic \;
   fi
fi

