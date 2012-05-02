//------------------------------------------------------------------------------
// Copyright (c) 2011 by European Organization for Nuclear Research (CERN)
// Author: Lukasz Janyst <ljanyst@cern.ch>
// See the LICENCE file for details.
//------------------------------------------------------------------------------

#ifndef XROOTD_PROTOCOL_HELPER_HH
#define XROOTD_PROTOCOL_HELPER_HH

#include <XrdCl/XrdClLog.hh>
#include <XrdCl/XrdClMessage.hh>

class XRootDProtocolHelper
{
  public:
    //--------------------------------------------------------------------------
    //! Handle XRootD Log-in
    //--------------------------------------------------------------------------
    bool HandleLogin( int socket, XrdCl::Log *log );

    //--------------------------------------------------------------------------
    //! Handle disconnection
    //--------------------------------------------------------------------------
    bool HandleClose( int socket, XrdCl::Log *log );

    //--------------------------------------------------------------------------
    //! Receive a message
    //--------------------------------------------------------------------------
    bool GetMessage( XrdCl::Message *msg, int socket, XrdCl::Log *log );
  private:
};

#endif // XROOTD_PROTOCOL_HELPER_HH
