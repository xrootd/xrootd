//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __CPPUNIT_XRD_HELPERS_HH__
#define __CPPUNIT_XRD_HELPERS_HH__

#include <XrdCl/XrdClXRootDResponses.hh>
#include <errno.h>
#include <string.h>

#define CPPUNIT_ASSERT_XRDST( x )                    \
{                                                    \
  XrdCl::XRootDStatus st = x;                    \
  std::string msg = "["; msg += #x; msg += "]: ";    \
  msg += st.ToStr();                                 \
  CPPUNIT_ASSERT_MESSAGE( msg, st.IsOK() );   \
}

#define CPPUNIT_ASSERT_ERRNO( x )                    \
{                                                    \
  std::string msg = "["; msg += #x; msg += "]: ";    \
  msg += strerror( errno );                          \
  CPPUNIT_ASSERT_MESSAGE( msg, x );                  \
}

#define CPPUNIT_ASSERT_PTHREAD( x )                  \
{                                                    \
  errno = x;                                         \
  std::string msg = "["; msg += #x; msg += "]: ";    \
  msg += strerror( errno );                          \
  CPPUNIT_ASSERT_MESSAGE( msg, errno == 0 );         \
}

#endif // __CPPUNIT_XRD_HELPERS_HH__
