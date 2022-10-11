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

#ifndef SRC_XRDCL_XRDCLASYNCRAWREADER_HH_
#define SRC_XRDCL_XRDCLASYNCRAWREADER_HH_


#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClStream.hh"
#include "XrdClAsyncRawReaderIntfc.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Object for reading out data from the kXR_read response
  //----------------------------------------------------------------------------
  class AsyncRawReader : public AsyncRawReaderIntfc
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param url     : channel URL
      //! @param request : client request
      //------------------------------------------------------------------------
      AsyncRawReader( const URL &url, const Message &request ) :
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
        while( true )
        {
          switch( readstage )
          {
            //------------------------------------------------------------------
            // Prepare to readout a new response
            //------------------------------------------------------------------
            case ReadStart:
            {
              msgbtsrd  = 0;
              chlen     = ( *chunks )[0].length;
              readstage = ReadRaw;
              continue;
            }

            //------------------------------------------------------------------
            // Readout the raw data
            //------------------------------------------------------------------
            case ReadRaw:
            {
              //----------------------------------------------------------------
              // Make sure we are not reading past the end of the read response
              //----------------------------------------------------------------
              if( msgbtsrd + chlen > dlen )
                chlen = dlen - msgbtsrd;

              //----------------------------------------------------------------
              // Readout the raw data from the socket
              //----------------------------------------------------------------
              uint32_t btsrd = 0;
              char *buff = static_cast<char*>( ( *chunks )[chidx].buffer );
              Status st = ReadBytesAsync( socket, buff + choff, chlen, btsrd );
              choff    += btsrd;
              chlen    -= btsrd;
              msgbtsrd += btsrd;
              rawbtsrd += btsrd;
              btsret   += btsrd;

              if( !st.IsOK() || st.code == suRetry )
                 return st;

              //----------------------------------------------------------------
              // If the chunk is full, move to the next buffer
              //----------------------------------------------------------------
              if( choff == ( *chunks )[chidx].length )
              {
                ++chidx;
                choff = 0;
                chlen = ( chidx < chunks->size() ? ( *chunks )[chidx].length : 0 );
              }
              //----------------------------------------------------------------
              // Check if there are some data left in the response to be readout
              // from the socket.
              //----------------------------------------------------------------
              if( msgbtsrd < dlen )
              {
                //--------------------------------------------------------------
                // We run out of space, the server has send too much data
                //--------------------------------------------------------------
                if( chidx >= chunks->size() )
                {
                  readstage = ReadDiscard;
                  continue;
                }
                readstage = ReadRaw;
                continue;
              }
              //----------------------------------------------------------------
              // We are done
              //----------------------------------------------------------------
              readstage = ReadDone;
              continue;
            }

            //------------------------------------------------------------------
            // We've had an error and we are in the discarding mode
            //------------------------------------------------------------------
            case ReadDiscard:
            {
              DefaultEnv::GetLog()->Error( XRootDMsg, "[%s] RawReader: Handling "
                                           "response to %s: user supplied buffer is "
                                           "too small for the received data.",
                                           url.GetHostId().c_str(),
                                           request.GetDescription().c_str() );
              // Just drop the connection, we don't know if the stream is sane
              // anymore. Recover with a reconnect.
              return XRootDStatus( stError, errCorruptedHeader );
            }

            //------------------------------------------------------------------
            // Finalize the read
            //------------------------------------------------------------------
            case ReadDone:
            {
              break;
            }

            //------------------------------------------------------------------
            // Others should not happen
            //------------------------------------------------------------------
            default : return XRootDStatus( stError, errInternal );
          }

          // just in case
          break;
        }
        //----------------------------------------------------------------------
        // We are done
        //----------------------------------------------------------------------
        return XRootDStatus();
      }

      //------------------------------------------------------------------------
      //! Get the response
      //------------------------------------------------------------------------
      XRootDStatus GetResponse( AnyObject *&response )
      {
        if( dataerr )
          return XRootDStatus( stError, errInvalidResponse );
        std::unique_ptr<AnyObject> rsp( new AnyObject() );
        if( request.GetVirtReqID() == kXR_virtReadv )
          rsp->Set( GetVectorReadInfo() );
        else
          rsp->Set( GetChunkInfo() );
        response = rsp.release();
        return XRootDStatus();
      }

    private:

      inline ChunkInfo* GetChunkInfo()
      {
        ChunkInfo *info = new ChunkInfo( chunks->front() );
        info->length = rawbtsrd;
        return info;
      }

      inline VectorReadInfo* GetVectorReadInfo()
      {
        VectorReadInfo *info = new VectorReadInfo();
        info->SetSize( rawbtsrd );
        int btsleft = rawbtsrd;
        for( auto &chunk : *chunks )
        {
          int length = uint32_t( btsleft ) >= chunk.length ? chunk.length : btsleft;
          info->GetChunks().emplace_back( chunk.offset, length, chunk.buffer );
          btsleft -= length;
        }
        return info;
      }
  };

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLASYNCVECTORREADER_HH_ */
