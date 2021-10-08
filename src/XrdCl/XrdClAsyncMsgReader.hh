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

#ifndef SRC_XRDCL_XRDCLASYNCMSGREADER_HH_
#define SRC_XRDCL_XRDCLASYNCMSGREADER_HH_

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
  //! Utility class encapsulating reading response message logic
  //----------------------------------------------------------------------------
  class AsyncMsgReader
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
      AsyncMsgReader( TransportHandler  &xrdTransport,
                       Socket            &socket,
                       const std::string &strmname,
                       Stream            &strm,
                       uint16_t           substrmnb) : readstage( ReadStart ),
                                                       xrdTransport( xrdTransport ),
                                                       socket( socket ),
                                                       strmname( strmname ),
                                                       strm( strm ),
                                                       substrmnb( substrmnb ),
                                                       incmsgsize( 0 ),
                                                       inchandler( std::make_pair( nullptr, false ) )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~AsyncMsgReader(){ }

      //------------------------------------------------------------------------
      //! Reset the state of the object (makes it ready to read out next msg)
      //------------------------------------------------------------------------
      inline void Reset()
      {
        readstage = ReadStart;
        incmsg.reset();
        incmsgsize = 0;
        inchandler = std::make_pair( nullptr, false );
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
              incmsg.reset( new Message() );
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
              XRootDStatus st = xrdTransport.GetHeader( incmsg.get(), &socket );
              if( !st.IsOK() || st.code == suRetry ) return st;


              log->Dump( AsyncSockMsg, "[%s] Received message header for 0x%x size: %d",
                        strmname.c_str(), incmsg.get(), incmsg->GetCursor() );
              incmsgsize = incmsg->GetCursor();
              inchandler = strm.InstallIncHandler( incmsg.get(), substrmnb );

              if( inchandler.first )
              {
                log->Dump( AsyncSockMsg, "[%s] Will use the raw handler to read body "
                           "of message 0x%x", strmname.c_str(), incmsg.get() );
                //--------------------------------------------------------------
                // The next step is to read raw data
                //--------------------------------------------------------------
                readstage = ReadRawData;
                continue;
              }
              //----------------------------------------------------------------
              // The next step is to read the message body
              //----------------------------------------------------------------
              readstage = ReadMsgBody;
              continue;
            }
            //------------------------------------------------------------------
            // We need to call a raw message handler to get the data from the
            // socket
            //------------------------------------------------------------------
            case ReadRawData:
            {
              uint32_t bytesRead = 0;
              XRootDStatus st = inchandler.first->ReadMessageBody( incmsg.get(), &socket, bytesRead );
              if( !st.IsOK() ) return st;
              incmsgsize += bytesRead;
              if( st.code == suRetry ) return st;
              //----------------------------------------------------------------
              // The next step is to finalize the read
              //----------------------------------------------------------------
              readstage = ReadDone;
              continue;
            }
            //------------------------------------------------------------------
            // No raw handler, so we read the message to the buffer
            //------------------------------------------------------------------
            case ReadMsgBody:
            {
              XRootDStatus st = xrdTransport.GetBody( incmsg.get(), &socket );
              if( !st.IsOK() || st.code == suRetry ) return st;
              incmsgsize = incmsg->GetCursor();

              //----------------------------------------------------------------
              // Now check if there are some additional raw data to be read
              //----------------------------------------------------------------
              if( !inchandler.first )
              {
                uint16_t action = strm.InspectStatusRsp( incmsg.get(), substrmnb,
                                                         inchandler.first );

                if( action & IncomingMsgHandler::Corrupted )
                  return XRootDStatus( stError, errCorruptedHeader );

                if( action & IncomingMsgHandler::Raw )
                {
                  inchandler.second = true;
                  //------------------------------------------------------------
                  // The next step is to read the raw data
                  //------------------------------------------------------------
                  readstage = ReadRawData;
                  continue;
                }

                if( action & IncomingMsgHandler::More )
                {
                  //------------------------------------------------------------
                  // The next step is to read the additional data in the message
                  // body
                  //------------------------------------------------------------
                  readstage = ReadMsgBody;
                  continue;
                }
              }
              //------------------------------------------------------------
              // The next step is to finalize the read
              //------------------------------------------------------------
              readstage = ReadDone;
              continue;
            }

            case ReadDone:
            {
              //----------------------------------------------------------------
              // Report the incoming message
              //----------------------------------------------------------------
              log->Dump( AsyncSockMsg, "[%s] Received message 0x%x of %d bytes",
                         strmname.c_str(), incmsg.get(), incmsgsize );

              strm.OnIncoming( substrmnb, incmsg.release(), incmsgsize );
            }
          }

          break;
        }

        //----------------------------------------------------------------------
        // We are done, so now reset the state so we can read next response
        //----------------------------------------------------------------------
        Reset();
        return XRootDStatus();
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
        ReadRawData, //< the next step is to read the raw data
        ReadDone     //< the next step is to finalize the read
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
      std::unique_ptr<Message>             incmsg;
      uint32_t                             incmsgsize;
      std::pair<IncomingMsgHandler*, bool> inchandler;

  };

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLASYNCMSGREADER_HH_ */
