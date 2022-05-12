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

#ifndef SRC_XRDCL_XRDCLASYNCVECTORREADER_HH_
#define SRC_XRDCL_XRDCLASYNCVECTORREADER_HH_

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdCl/XrdClStream.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Object for reading out data from the VectorRead response
  //----------------------------------------------------------------------------
  class AsyncVectorReader
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param url : channel URL
      //------------------------------------------------------------------------
      AsyncVectorReader( const URL &url ) :
        readstage( ReadStart ),
        url( url ),
        chunks( nullptr ),
        dlen( 0 ),
        totalbtsrd( 0 ),
        chidx( 0 ),
        choff( 0 ),
        chlen( 0 ),
        rdlstoff( 0 ),
        rdlstlen( 0 )
      {
        memset( &rdlst, 0, sizeof( readahead_list ) );
      }

      //------------------------------------------------------------------------
      //! Destructor
      //------------------------------------------------------------------------
      virtual ~AsyncVectorReader()
      {
      }

      //------------------------------------------------------------------------
      //! Sets response data length
      //------------------------------------------------------------------------
      void SetDataLength( int dlen )
      {
        this->dlen       = dlen;
        this->totalbtsrd = 0;
        this->readstage  = ReadStart;
      }

      //------------------------------------------------------------------------
      //! Sets the chunk list with user buffers
      //------------------------------------------------------------------------
      void SetChunkList( ChunkList *chunks )
      {
        this->chunks = chunks;
        this->chstatus.resize( chunks->size() );
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
              rdlstoff  = 0;
              rdlstlen  = sizeof( readahead_list );
              readstage = ReadRdLst;
              continue;
            }

            //------------------------------------------------------------------
            // Readout the read_list
            //------------------------------------------------------------------
            case ReadRdLst:
            {
              //----------------------------------------------------------------
              // We cannot afford to read the next header from the stream
              // because we will cross the message boundary
              //----------------------------------------------------------------
              if( totalbtsrd + rdlstlen > dlen )
              {
                uint32_t btsleft = dlen - totalbtsrd;
                log->Error( XRootDMsg, "[%s] ReadRawReadV: No enough data to read "
                            "another chunk header. Discarding %d bytes.",
                            url.GetHostId().c_str(), btsleft );
                readstage = ReadDiscard;
                continue;
              }

              //----------------------------------------------------------------
              // Let's readout the read list record from the socket
              //----------------------------------------------------------------
              uint32_t btsrd = 0;
              char *buff = reinterpret_cast<char*>( &rdlst );
              Status st = ReadBytesAsync( socket, buff + rdlstoff, rdlstlen, btsrd );
              rdlstoff   += btsrd;
              rdlstlen   -= btsrd;
              totalbtsrd += btsrd;
              btsret     += btsrd;

              if( !st.IsOK() || st.code == suRetry )
                 return st;

              //----------------------------------------------------------------
              // We have a complete read list record, now we need to marshal it
              //----------------------------------------------------------------
              rdlst.rlen   = ntohl( rdlst.rlen );
              rdlst.offset = ntohll( rdlst.offset );
              choff = 0;
              chlen = rdlst.rlen;

              //----------------------------------------------------------------
              // Find the buffer corresponding to the chunk
              //----------------------------------------------------------------
              bool chfound = false;
              for( size_t i = 0; i < chunks->size(); ++i )
              {
                if( ( *chunks )[i].offset == uint64_t( rdlst.offset ) &&
                    ( *chunks )[i].length == uint32_t( rdlst.rlen ) )
                {
                  chfound = true;
                  chidx = i;
                  break;
                }
              }

              //----------------------------------------------------------------
              // If the chunk was not found this is a bogus response, switch
              // to discard mode
              //----------------------------------------------------------------
              if( !chfound )
              {
                log->Error( XRootDMsg, "[%s] ReadRawReadV: Impossible to find chunk "
                            "buffer corresponding to %d bytes at %ld",
                            url.GetHostId().c_str(), rdlst.rlen, rdlst.offset );
                uint32_t btsleft = dlen - totalbtsrd;
                log->Dump( XRootDMsg, "[%s] ReadRawReadV: Discarding %d bytes",
                           url.GetHostId().c_str(), btsleft );
                readstage = ReadDiscard;
                continue;
              }

              readstage = ReadChunk;
              continue;
            }

            //------------------------------------------------------------------
            // Readout the raw data
            //------------------------------------------------------------------
            case ReadChunk:
            {
              //----------------------------------------------------------------
              // The chunk was found, but reading all the data will cross the
              // message boundary
              //----------------------------------------------------------------
              if( totalbtsrd + chlen > dlen )
              {
                uint32_t btsleft = dlen - totalbtsrd;
                log->Error( XRootDMsg, "[%s] ReadRawReadV: Malformed chunk header: "
                            "reading %d bytes from message would cross the message "
                            "boundary, discarding %d bytes.", url.GetHostId().c_str(),
                            rdlst.rlen, btsleft );
                chstatus[chidx].sizeerr = true;
                readstage = ReadDiscard;
                continue;
              }

              //----------------------------------------------------------------
              // Readout the raw data from the socket
              //----------------------------------------------------------------
              uint32_t btsrd = 0;
              char *buff = static_cast<char*>( ( *chunks )[chidx].buffer );
              Status st = ReadBytesAsync( socket, buff + choff, chlen, btsrd );
              choff      += btsrd;
              chlen      -= btsrd;
              totalbtsrd += btsrd;
              btsret     += btsrd;

              if( !st.IsOK() || st.code == suRetry )
                 return st;

              log->Dump( XRootDMsg, "[%s] ReadRawReadV: read buffer for chunk %d@%ld",
                         url.GetHostId().c_str(), rdlst.rlen, rdlst.offset );

              //----------------------------------------------------------------
              // Mark chunk as done
              //----------------------------------------------------------------
              chstatus[chidx].done = true;

              //----------------------------------------------------------------
              // There is still data to be read, we need to readout the next
              // read list record.
              //----------------------------------------------------------------
              if( totalbtsrd < dlen )
              {
                rdlstoff  = 0;
                rdlstlen  = sizeof( readahead_list );
                readstage = ReadRdLst;
                continue;
              }

              readstage = ReadDone;
              continue;
            }

            //------------------------------------------------------------------
            // We've had an error and we are in the discarding mode
            //------------------------------------------------------------------
            case ReadDiscard:
            {
              uint32_t btsleft = dlen - totalbtsrd;
              // allocate the discard buffer if necessary
              if( discardbuff.size() < btsleft )
                discardbuff.resize( btsleft );

              //----------------------------------------------------------------
              // We need to readout the data from the socket in order to keep
              // the stream sane.
              //----------------------------------------------------------------
              uint32_t btsrd = 0;
              Status st = ReadBytesAsync( socket, discardbuff.data(), btsleft, btsrd );
              totalbtsrd += btsrd;
              btsret     += btsrd;

              log->Warning( XRootDMsg, "[%s] ReadRawReadV: Discarded %d bytes",
                            url.GetHostId().c_str(), btsrd );

              if( !st.IsOK() || st.code == suRetry )
                return st;

              readstage = ReadDone;
              continue;
            }

            //------------------------------------------------------------------
            // Finalize the read
            //------------------------------------------------------------------
            case ReadDone:
            {
              chidx = 0;
              choff = 0;
              chlen = 0;
              rdlstoff = 0;
              rdlstlen = 0;
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

      Status GetVectorReadInfo( VectorReadInfo *&info )
      {
        //--------------------------------------------------------------------------
        // See if all the chunks are OK and put them in the response
        //--------------------------------------------------------------------------
        std::unique_ptr<VectorReadInfo> ptr( new VectorReadInfo() );
        uint32_t size = 0;
        for( uint32_t i = 0; i < chunks->size(); ++i )
        {
          if( !chstatus[i].done )
             return Status( stFatal, errInvalidResponse );
          ptr->GetChunks().emplace_back( ( *chunks )[i].offset,
              ( *chunks )[i].length, ( *chunks )[i].buffer );
          size += ( *chunks )[i].length;
        }
        ptr->SetSize( size );
        info = ptr.release();
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
        ReadRdLst,   //< the next step is to read the read_list
        ReadChunk,   //< the next step is to read the raw data
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

      ChunkList                *chunks;       //< list of data chunks to be filled with user data
      std::vector<ChunkStatus>  chstatus;     //< status per chunk
      uint32_t                  dlen;         //< size of the data in the message
      uint32_t                  totalbtsrd;   //< total number of bytes read out from the socket

      size_t                    chidx;        //< index of the current data buffer
      size_t                    choff;        //< offset within the current buffer
      size_t                    chlen;        //< bytes left to be readout into the current chunk

      size_t                    rdlstoff;     //< offset within the current read_list
      readahead_list            rdlst;        //< the readahead list for the current chunk
      size_t                    rdlstlen;     //< bytes left to be readout into read list

      buffer_t                  discardbuff;  //< buffer for discarding data in case of an error
  };

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLASYNCVECTORREADER_HH_ */
