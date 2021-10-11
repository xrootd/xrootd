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

#ifndef SRC_XRDCL_XRDCLASYNCHSREADER_HH_
#define SRC_XRDCL_XRDCLASYNCHSREADER_HH_

#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClStream.hh"

#include <memory>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Utility class encapsulating reading hand-shake response logic
  //----------------------------------------------------------------------------
  class AsyncHSReader
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param xrdTransport : the (xrootd) transport layer
      //! @param socket       : the socket with the message to be read out
      //! @param strmname     : stream name
      //! @param strm         : the stream encapsulating the connection
      //! @param substrmnb    : the substream number
      //------------------------------------------------------------------------
      AsyncHSReader( TransportHandler  &xrdTransport,
                     Socket            &socket,
                     const std::string &strmname,
                     Stream            &strm,
                     uint16_t           substrmnb) : readstage( ReadStart ),
                                                     xrdTransport( xrdTransport ),
                                                     socket( socket ),
                                                     strmname( strmname ),
                                                     strm( strm ),
                                                     substrmnb( substrmnb )
      {
      }

      //------------------------------------------------------------------------
      //! Read out the response from the socket
      //------------------------------------------------------------------------
      XRootDStatus Read()
      {
        Log  *log = DefaultEnv::GetLog();

        while( true )
        {
          switch( readstage )
          {
            //------------------------------------------------------------------
            // There is no incoming message currently being processed so we
            // create a new one
            //------------------------------------------------------------------
            case ReadStart:
            {
              inmsg.reset( new Message() );
              //----------------------------------------------------------------
              // The next step is to read the header
              //----------------------------------------------------------------
              readstage = ReadHeader;
              continue;
            }
            //------------------------------------------------------------------
            // We need to read the header
            //------------------------------------------------------------------
            case ReadHeader:
            {
              XRootDStatus st = xrdTransport.GetHeader( *inmsg, &socket );
              if( !st.IsOK() || st.code == suRetry ) return st;
              log->Dump( AsyncSockMsg,
                        "[%s] Received message header, size: %d",
                        strmname.c_str(), inmsg->GetCursor() );
              //----------------------------------------------------------------
              // The next step is to read the message body
              //----------------------------------------------------------------
              readstage = ReadMsgBody;
              continue;
            }
            //------------------------------------------------------------------
            // We read the message to the buffer
            //------------------------------------------------------------------
            case ReadMsgBody:
            {
              XRootDStatus st = xrdTransport.GetBody( *inmsg, &socket );
              if( !st.IsOK() || st.code == suRetry ) return st;
              log->Dump( AsyncSockMsg, "[%s] Received a message of %d bytes",
                         strmname.c_str(), inmsg->GetSize() );
              readstage = ReadDone;
              return st;
            }

            case ReadDone: return XRootDStatus();
          }
          // just in case ...
          break;
        }
        //----------------------------------------------------------------------
        // We are done
        //----------------------------------------------------------------------
        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Transfer the received message ownership
      //------------------------------------------------------------------------
      std::unique_ptr<Message> ReleaseMsg()
      {
        readstage = ReadStart;
        return std::move( inmsg );
      }

      //------------------------------------------------------------------------
      //! Reset the state of the object (makes it ready to read out next msg)
      //------------------------------------------------------------------------
      inline void Reset()
      {
        readstage = ReadStart;
        inmsg.reset();
      }

    private:

      //------------------------------------------------------------------------
      //! Stages of reading out a response from the socket
      //------------------------------------------------------------------------
      enum Stage
      {
        ReadStart,   //< the next step is to initialize the read
        ReadHeader,  //< the next step is to read the header
        ReadMsgBody, //< the next step is to read the body
        ReadDone     //< we are done
      };

      //------------------------------------------------------------------------
      // Current read stage
      //------------------------------------------------------------------------
      Stage readstage;

      //------------------------------------------------------------------------
      // The context of the read operation
      //------------------------------------------------------------------------
      TransportHandler  &xrdTransport;
      Socket            &socket;
      const std::string &strmname;
      Stream            &strm;
      uint16_t           substrmnb;

      //------------------------------------------------------------------------
      // The internal state of the the reader
      //------------------------------------------------------------------------
      std::unique_ptr<Message>  inmsg;
  };
}

#endif /* SRC_XRDCL_XRDCLASYNCHSREADER_HH_ */
