//------------------------------------------------------------------------------
// Copyright (c) 2011-2012 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
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

#ifndef SRC_XRDCL_XRDCLASYNCDISCARDREADER_HH_
#define SRC_XRDCL_XRDCLASYNCDISCARDREADER_HH_

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdClAsyncRawReaderIntfc.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Object for discarding data
  //----------------------------------------------------------------------------
  class AsyncDiscardReader : public AsyncRawReaderIntfc
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param url     : channel URL
      //! @param request : client request
      //------------------------------------------------------------------------
      AsyncDiscardReader( const URL &url, const Message &request ) :
        AsyncRawReaderIntfc( url, request )
      {
      }

      //------------------------------------------------------------------------
      //! Readout raw data from socket
      //!
      //! @param socket : the socket
      //! @param btsret : number of bytes read
      //! @return       : operation status
      //------------------------------------------------------------------------
      XRootDStatus Read( Socket &socket, uint32_t &btsret )
      {
        Log  *log = DefaultEnv::GetLog();
        log->Error( XRootDMsg, "[%s] Handling response to %s: "
                               "DiscardReader: we were not expecting "
                               "raw data.", url.GetHostId().c_str(),
                               request.GetDescription().c_str() );
        // Just drop the connection, we don't know if the stream is sane anymore.
        // Recover with a reconnect.
        return XRootDStatus( stError, errCorruptedHeader );
      }

      //------------------------------------------------------------------------
      //! Get the response, since we received some unexpected data we always
      //! return an error to the end user.
      //------------------------------------------------------------------------
      XRootDStatus GetResponse( AnyObject *&response )
      {
        response = nullptr;
        return XRootDStatus( stError, errInvalidResponse );
      }
  };

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLASYNCVECTORREADER_HH_ */
