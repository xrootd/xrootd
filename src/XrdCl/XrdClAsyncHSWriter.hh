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

#ifndef SRC_XRDCL_XRDCLASYNCHSWRITER_HH_
#define SRC_XRDCL_XRDCLASYNCHSWRITER_HH_

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
  //! Utility class encapsulating writing hand-shake request logic
  //----------------------------------------------------------------------------
  class AsyncHSWriter
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param socket       : the socket with the message to be read out
      //! @param strmname     : stream name
      //------------------------------------------------------------------------
      AsyncHSWriter( Socket            &socket,
                     const std::string &strmname ) : writestage( WriteRequest ),
                                                     socket( socket ),
                                                     strmname( strmname ),
                                                     outmsg( nullptr )
      {
      }

      //------------------------------------------------------------------------
      //! Reset the state of the object (makes it ready to read out next msg)
      //------------------------------------------------------------------------
      inline void Reset( Message *msg = nullptr )
      {
        writestage = WriteRequest;
        outmsg.reset( msg );
      }

      //------------------------------------------------------------------------
      //! Replay the message that has been sent
      //------------------------------------------------------------------------
      inline void Replay()
      {
        if( !outmsg ) return;
        writestage = WriteRequest;
        outmsg->SetCursor( 0 );
      }

      //------------------------------------------------------------------------
      //! Check if writer was assigned with a message
      //------------------------------------------------------------------------
      inline bool HasMsg()
      {
        return bool( outmsg );
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
            case WriteRequest:
            {
              XRootDStatus st = socket.Send( *outmsg, strmname );
              if( !st.IsOK() || st.code == suRetry ) return st;
              //----------------------------------------------------------------
              // The next step is to write the signature
              //----------------------------------------------------------------
              writestage = WriteDone;
              continue;
            }

            case WriteDone:
            {
              XRootDStatus st = socket.Flash();
              if( !st.IsOK() )
              {
                log->Error( AsyncSockMsg, "[%s] Unable to flash the socket: %s",
                            strmname.c_str(), XrdSysE2T( st.errNo ) );
              }
              return st;
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
        WriteRequest, //< the next step is to write the request
        WriteDone     //< the next step is to finalize the write
      };

      //------------------------------------------------------------------------
      // Current read stage
      //------------------------------------------------------------------------
      Stage writestage;

      //------------------------------------------------------------------------
      // The context of the read operation
      //------------------------------------------------------------------------
      Socket            &socket;
      const std::string &strmname;

      //------------------------------------------------------------------------
      // The internal state of the the reader
      //------------------------------------------------------------------------
      std::unique_ptr<Message>  outmsg;
  };
}


#endif /* SRC_XRDCL_XRDCLASYNCHSWRITER_HH_ */
