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

#ifndef SRC_XRDCL_XRDCLASYNCMSGWRITER_HH_
#define SRC_XRDCL_XRDCLASYNCMSGWRITER_HH_

#include "XrdCl/XrdClMessage.hh"
#include "XrdCl/XrdClPostMasterInterfaces.hh"
#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClConstants.hh"
#include "XrdCl/XrdClStream.hh"
#include "XrdSys/XrdSysE2T.hh"

#include <memory>

namespace XrdCl
{
  //----------------------------------------------------------------------------
  //! Utility class encapsulating writing request logic
  //----------------------------------------------------------------------------
  class AsyncMsgWriter
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
      AsyncMsgWriter( TransportHandler  &xrdTransport,
                      Socket            &socket,
                      const std::string &strmname,
                      Stream            &strm,
                      uint16_t           substrmnb,
                      AnyObject         &chdata ) : writestage( WriteStart ),
                                                    xrdTransport( xrdTransport ),
                                                    socket( socket ),
                                                    strmname( strmname ),
                                                    strm( strm ),
                                                    substrmnb( substrmnb ),
                                                    chdata( chdata ),
                                                    outmsg( nullptr ),
                                                    outmsgsize( 0 ),
                                                    outhandler( nullptr )
      {
      }

      //------------------------------------------------------------------------
      //! Reset the state of the object (makes it ready to read out next msg)
      //------------------------------------------------------------------------
      inline void Reset()
      {
        writestage = WriteStart;
        outmsg     = nullptr;
        outmsgsize = 0;;
        outhandler = nullptr;
        outsign.reset();
      }

      //------------------------------------------------------------------------
      //! Write the request into the socket
      //------------------------------------------------------------------------
      XRootDStatus Write()
      {
        Log *log = DefaultEnv::GetLog();
        while( true )
        {
          switch( writestage )
          {
            //------------------------------------------------------------------
            // Pick up a message if we're not in process of writing something
            //------------------------------------------------------------------
            case WriteStart:
            {
              std::pair<Message *, MsgHandler *> toBeSent;
              toBeSent = strm.OnReadyToWrite( substrmnb );
              outmsg = toBeSent.first;
              outhandler = toBeSent.second;
              if( !outmsg ) return XRootDStatus( stOK, suAlreadyDone );

              outmsg->SetCursor( 0 );
              outmsgsize = outmsg->GetSize();

              //----------------------------------------------------------------
              // Secure the message if necessary
              //----------------------------------------------------------------
              Message *signature = nullptr;
              XRootDStatus st = xrdTransport.GetSignature( outmsg, signature, chdata );
              if( !st.IsOK() ) return st;
              outsign.reset( signature );

              if( outsign )
                outmsgsize += outsign->GetSize();

              //----------------------------------------------------------------
              // The next step is to write the signature
              //----------------------------------------------------------------
              writestage = WriteSign;
              continue;
            }
            //------------------------------------------------------------------
            // First write the signature (if there is one)
            //------------------------------------------------------------------
            case WriteSign:
            {
              //----------------------------------------------------------------
              // If there is a signature for the request send it over the socket
              //----------------------------------------------------------------
              if( outsign )
              {
                XRootDStatus st = socket.Send( *outsign, strmname );
                if( !st.IsOK() || st.code == suRetry ) return st;
              }
              //----------------------------------------------------------------
              // The next step is to write the signature
              //----------------------------------------------------------------
              writestage = WriteRequest;
              continue;
            }
            //------------------------------------------------------------------
            // Then write the request itself
            //------------------------------------------------------------------
            case WriteRequest:
            {
              XRootDStatus st = socket.Send( *outmsg, strmname );
              if( !st.IsOK() || st.code == suRetry ) return st;
              //----------------------------------------------------------------
              // The next step is to write the signature
              //----------------------------------------------------------------
              writestage = WriteRawData;
              continue;
            }
            //------------------------------------------------------------------
            // And then write the raw data (if any)
            //------------------------------------------------------------------
            case WriteRawData:
            {
              if( outhandler->IsRaw() )
              {
                uint32_t wrtcnt = 0;
                XRootDStatus st = outhandler->WriteMessageBody( &socket, wrtcnt );
                if( !st.IsOK() || st.code == suRetry ) return st;
                outmsgsize += wrtcnt;
                log->Dump( AsyncSockMsg, "[%s] Wrote %d bytes of raw data of message"
                           "(0x%x) body.", strmname.c_str(), wrtcnt, outmsg );
              }
              //----------------------------------------------------------------
              // The next step is to finalize the write operation
              //----------------------------------------------------------------
              writestage = WriteDone;
              continue;
            }
            //------------------------------------------------------------------
            // Finally, finalize the write operation
            //------------------------------------------------------------------
            case WriteDone:
            {
              XRootDStatus st = socket.Flash();
              if( !st.IsOK() )
              {
                log->Error( AsyncSockMsg, "[%s] Unable to flash the socket: %s",
                            strmname.c_str(), XrdSysE2T( st.errNo ) );
                return st;
              }

              log->Dump( AsyncSockMsg, "[%s] Successfully sent message: %s (0x%x).",
                         strmname.c_str(), outmsg->GetDescription().c_str(), outmsg );

              strm.OnMessageSent( substrmnb, outmsg, outmsgsize );
              return XRootDStatus();
            }
          }
          // just in case ...
          break;
        }
        //----------------------------------------------------------------------
        // We are done
        //----------------------------------------------------------------------
        return XRootDStatus();
      }

    private:

      //------------------------------------------------------------------------
      //! Stages of reading out a response from the socket
      //------------------------------------------------------------------------
      enum Stage
      {
        WriteStart,   //< the next step is to initialize the read
        WriteSign,    //< the next step is to write the signature
        WriteRequest, //< the next step is to write the request
        WriteRawData, //< the next step is to write the raw data
        WriteDone     //< the next step is to finalize the write
      };

      //------------------------------------------------------------------------
      // Current read stage
      //------------------------------------------------------------------------
      Stage writestage;

      //------------------------------------------------------------------------
      // The context of the read operation
      //------------------------------------------------------------------------
      TransportHandler  &xrdTransport;
      Socket            &socket;
      const std::string &strmname;
      Stream            &strm;
      uint16_t           substrmnb;
      AnyObject         &chdata;

      //------------------------------------------------------------------------
      // The internal state of the the reader
      //------------------------------------------------------------------------
      Message                  *outmsg; //< we don't own the message
      uint32_t                  outmsgsize;
      MsgHandler               *outhandler;
      std::unique_ptr<Message>  outsign;
  };

}

#endif /* SRC_XRDCL_XRDCLASYNCMSGWRITER_HH_ */
