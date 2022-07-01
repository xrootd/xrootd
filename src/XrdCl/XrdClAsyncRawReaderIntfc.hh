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

#ifndef SRC_XRDCL_XRDCLASYNCRAWREADERINTFC_HH_
#define SRC_XRDCL_XRDCLASYNCRAWREADERINTFC_HH_

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClDefaultEnv.hh"
#include "XrdCl/XrdClLog.hh"
#include "XrdCl/XrdClConstants.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Base class for any message's body reader
  //----------------------------------------------------------------------------
  class AsyncRawReaderIntfc
  {
    public:

      AsyncRawReaderIntfc( const URL &url, const Message &request ) :
        readstage( ReadStart ),
        url( url ),
        request( request ),
        chunks( nullptr ),
        dlen( 0 ),
        msgbtsrd( 0 ),
        rawbtsrd( 0 ),
        chidx( 0 ),
        choff( 0 ),
        chlen( 0 ),
        dataerr( false )
      {
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~AsyncRawReaderIntfc()
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
        if( chunks )
          this->chstatus.resize( chunks->size() );
      }

      //------------------------------------------------------------------------
      //! Readout raw data from socket
      //!
      //! @param socket : the socket
      //! @param btsret : number of bytes read
      //! @return       : operation status
      //------------------------------------------------------------------------
      virtual XRootDStatus Read( Socket &socket, uint32_t &btsret ) = 0;

      //------------------------------------------------------------------------
      //! Get the response
      //------------------------------------------------------------------------
      virtual XRootDStatus GetResponse( AnyObject *&response ) = 0;

    protected:

      //--------------------------------------------------------------------------
      // Read a buffer asynchronously
      //--------------------------------------------------------------------------
      XRootDStatus ReadBytesAsync( Socket   &socket,
                                   char     *buffer,
                                   uint32_t  toBeRead,
                                   uint32_t &bytesRead )
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
        return XRootDStatus( stOK, suDone );
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
        ReadRdLst,   //< the next step is to read the read_list
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
      const URL                &url;          //< for logging purposes
      const Message            &request;      //< client request

      ChunkList                *chunks;       //< list of data chunks to be filled with user data
      std::vector<ChunkStatus>  chstatus;     //< status per chunk
      uint32_t                  dlen;         //< size of the data in the message
      uint32_t                  msgbtsrd;     //< number of bytes read out from the socket for the current message
      uint32_t                  rawbtsrd;     //< total number of bytes read out from the socket (raw data only)

      size_t                    chidx;        //< index of the current data buffer
      size_t                    choff;        //< offset within the current buffer
      size_t                    chlen;        //< bytes left to be readout into the current chunk

      buffer_t                  discardbuff;  //< buffer for discarding data in case of an error
      bool                      dataerr;      //< true if the server send us too much data, false otherwise
  };

}

#endif /* SRC_XRDCL_XRDCLASYNCRAWREADERINTFC_HH_ */
