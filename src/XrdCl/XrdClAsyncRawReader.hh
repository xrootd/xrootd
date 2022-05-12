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

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Object for reading out data from the VectorRead response
  //----------------------------------------------------------------------------
  class AsyncRawReader
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param url : channel URL
      //------------------------------------------------------------------------
      AsyncRawReader( const URL &url, const Message &request ) :
        readstage( ReadStart ),
        url( url ),
        request( request ),
        chunks( nullptr ),
        dlen( 0 ),
        msgbtsrd( 0 ),
        totalbtsrd( 0 ),
        chidx( 0 ),
        choff( 0 ),
        chlen( 0 ),
        sizeerr( false )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~AsyncRawReader()
      {
      }

      //------------------------------------------------------------------------
      //! Sets response data length
      //------------------------------------------------------------------------
      void SetDataLength( int dlen )
      {
        this->dlen       = dlen;
        this->readstage  = ReadStart;
      }

      //------------------------------------------------------------------------
      //! Sets the chunk list with user buffers
      //------------------------------------------------------------------------
      void SetChunkList( ChunkList *chunks )
      {
        this->chunks = chunks;
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
              choff        += btsrd;
              chlen        -= btsrd;
              msgbtsrd     += btsrd;
              totalbtsrd += btsrd;
              btsret       += btsrd;

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
                if( choff == ( *chunks )[chidx].length )
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
              sizeerr = true;
              uint32_t btsleft = dlen - msgbtsrd;
              // allocate the discard buffer if necessary
              if( discardbuff.size() < btsleft )
                discardbuff.resize( btsleft );

              //----------------------------------------------------------------
              // We need to readout the data from the socket in order to keep
              // the stream sane.
              //----------------------------------------------------------------
              uint32_t btsrd = 0;
              Status st = ReadBytesAsync( socket, discardbuff.data(), btsleft, btsrd );
              msgbtsrd += btsrd;
              btsret     += btsrd;

              log->Warning( XRootDMsg, "[%s] ReadRawRead: Discarded %d bytes",
                            url.GetHostId().c_str(), btsrd );

              if( !st.IsOK() || st.code == suRetry )
                return st;

              DefaultEnv::GetLog()->Error( XRootDMsg, "[%s] Handling response to %s: "
                                           "user supplied buffer is too small for the "
                                           "received data.", url.GetHostId().c_str(),
                                           request.GetDescription().c_str() );
              readstage = ReadDone;
              continue;
            }

            //------------------------------------------------------------------
            // Finalize the read
            //------------------------------------------------------------------
            case ReadDone:
            {
              break;
            }
          }

          // just in case
          break;
        }
        //----------------------------------------------------------------------
        // We are done
        //----------------------------------------------------------------------
        return XRootDStatus();
      }

      Status GetChunkInfo( ChunkInfo *&info )
      {
        if( sizeerr )
          return Status( stError, errInvalidResponse );
        info = new ChunkInfo( chunks->front() );
        info->length = totalbtsrd;
        return Status();

      }

      Status GetVectorReadInfo( VectorReadInfo *&info )
      {
        if( sizeerr )
          return Status( stError, errInvalidResponse );
        info = new VectorReadInfo();
        info->SetSize( totalbtsrd );
        int btsleft = totalbtsrd;
        for( auto &chunk : *chunks )
        {
          int length = uint32_t( btsleft ) >= chunk.length ? chunk.length : btsleft;
          info->GetChunks().emplace_back( chunk.offset, length, chunk.buffer );
          btsleft -= length;
        }
        return Status();
      }

    private:

      //--------------------------------------------------------------------------
      // Read a buffer asynchronously - depends on pAsyncBuffer, pAsyncSize
      // and pAsyncOffset
      //--------------------------------------------------------------------------
      Status ReadBytesAsync( Socket &socket, char *buffer, uint32_t toBeRead, uint32_t &bytesRead )
      {
        size_t shift = 0;
        while( toBeRead > 0 )
        {
          int btsRead = 0;
          Status status = socket.Read( buffer + shift, toBeRead, btsRead );

          if( !status.IsOK() || status.code == suRetry )
            return status;

          bytesRead += btsRead;
          toBeRead  -= btsRead;
          shift     += btsRead;
        }
        return Status( stOK, suDone );
      }

      //------------------------------------------------------------------------
      // Helper struct for async reading of chunks
      //------------------------------------------------------------------------
      struct ChunkStatus
      {
        ChunkStatus(): sizeerr( false ), done( false ) {}
        bool sizeerr;
        bool done;
      };

      //------------------------------------------------------------------------
      // internal buffer type
      //------------------------------------------------------------------------
      using buffer_t = std::vector<char>;

      //------------------------------------------------------------------------
      //! Stages of reading out a response from the socket
      //------------------------------------------------------------------------
      enum Stage
      {
        ReadStart,   //< the next step is to initialize the read
        ReadRaw,     //< the next step is to read the raw data
        ReadDiscard, //< there was an error, we are in discard mode
        ReadDone     //< the next step is to finalize the read
      };

      //------------------------------------------------------------------------
      // Current read stage
      //------------------------------------------------------------------------
      Stage readstage;

      //------------------------------------------------------------------------
      // The context of the read operation
      //------------------------------------------------------------------------
      const URL     &url;          //< for logging purposes
      const Message &request;      //< client request

      ChunkList     *chunks;       //< list of data chunks to be filled with user data
      uint32_t       dlen;         //< size of the data in the message
      uint32_t       msgbtsrd;     //< number of bytes read out from the socket for the current message
      uint32_t       totalbtsrd;   //< total number of bytes read out from the socket

      size_t         chidx;        //< index of the current data buffer
      size_t         choff;        //< offset within the current buffer
      size_t         chlen;        //< bytes left to be readout into the current chunk

      buffer_t       discardbuff;  //< buffer for discarding data in case of an error
      bool           sizeerr;      //< true if the server send us too much data, false otherwise
  };

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLASYNCVECTORREADER_HH_ */
