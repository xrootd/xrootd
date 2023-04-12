//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
//------------------------------------------------------------------------------
// XRootD is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XRootD is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with XRootD.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------

#ifndef __CPPUNIT_XRD_HELPERS_HH__
#define __CPPUNIT_XRD_HELPERS_HH__

#include <XrdCl/XrdClXRootDResponses.hh>
#include <cerrno>
#include <cstring>

#define CPPUNIT_ASSERT_XRDST_NOTOK( x, err )         \
{                                                    \
  XrdCl::XRootDStatus _st = x;                       \
  std::string msg = "["; msg += #x; msg += "]: ";    \
  msg += _st.ToStr();                                \
  CPPUNIT_ASSERT_MESSAGE( msg, !_st.IsOK() && _st.code == err ); \
}

#define CPPUNIT_ASSERT_XRDST( x )                    \
{                                                    \
  XrdCl::XRootDStatus _st = x;                       \
  std::string msg = "["; msg += #x; msg += "]: ";    \
  msg += _st.ToStr();                                \
  CPPUNIT_ASSERT_MESSAGE( msg, _st.IsOK() );         \
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
