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
    bool HandleLogin( int socket, XrdClient::Log *log );

    //--------------------------------------------------------------------------
    //! Handle disconnection
    //--------------------------------------------------------------------------
    bool HandleClose( int socket, XrdClient::Log *log );

    //--------------------------------------------------------------------------
    //! Receive a message
    //--------------------------------------------------------------------------
    bool GetMessage( XrdClient::Message *msg, int socket, XrdClient::Log *log );
  private:
};

#endif // XROOTD_PROTOCOL_HELPER_HH
