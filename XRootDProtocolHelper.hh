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
