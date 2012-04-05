//------------------------------------------------------------------------------
// Copyright (c) 2012 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef __CPPUNIT_XRD_HELPERS_HH__
#define __CPPUNIT_XRD_HELPERS_HH__

#define CPPUNIT_ASSERT_XRDST( x )                    \
{                                                    \
  XRootDStatus st = x;                               \
  std::string msg = "["; msg += #x; msg += "]: ";    \
  msg += st.ToStr();                                 \
  CPPUNIT_ASSERT_MESSAGE( msg, st.IsOK() );   \
}

#endif // __CPPUNIT_XRD_HELPERS_HH__
