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
#include "XrdClAsyncRawReaderIntfc.hh"

namespace XrdCl
{

  //----------------------------------------------------------------------------
  //! Object for reading out data from the VectorRead response
  //----------------------------------------------------------------------------
  class AsyncVectorReader : public AsyncRawReaderIntfc
  {
    public:
      //------------------------------------------------------------------------
      //! Constructor
      //!
      //! @param url : channel URL
      //------------------------------------------------------------------------
      AsyncVectorReader( const URL &url, const Message &request ) :
        AsyncRawReaderIntfc( url, request ),
        rdlstoff( 0 ),
        rdlstlen( 0 )
      {
        memset( &rdlst, 0, sizeof( readahead_list ) );
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
        Log *log = DefaultEnv::GetLog();

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
              if( msgbtsrd + rdlstlen > dlen )
              {
                uint32_t btsleft = dlen - msgbtsrd;
                log->Error( XRootDMsg, "[%s] VectorReader: No enough data to read "
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
              rdlstoff += btsrd;
              rdlstlen -= btsrd;
              msgbtsrd += btsrd;
              btsret   += btsrd;

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
                log->Error( XRootDMsg, "[%s] VectorReader: Impossible to find chunk "
                            "buffer corresponding to %d bytes at %ld",
                            url.GetHostId().c_str(), rdlst.rlen, rdlst.offset );
                readstage = ReadDiscard;
                continue;
              }

              readstage = ReadRaw;
              continue;
            }

            //------------------------------------------------------------------
            // Readout the raw data
            //------------------------------------------------------------------
            case ReadRaw:
            {
              //----------------------------------------------------------------
              // The chunk was found, but reading all the data will cross the
              // message boundary
              //----------------------------------------------------------------
              if( msgbtsrd + chlen > dlen )
              {
                uint32_t btsleft = dlen - msgbtsrd;
                log->Error( XRootDMsg, "[%s] VectorReader: Malformed chunk header: "
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
              choff    += btsrd;
              chlen    -= btsrd;
              msgbtsrd += btsrd;
              rawbtsrd += btsrd;
              btsret   += btsrd;

              if( !st.IsOK() || st.code == suRetry )
                 return st;

              log->Dump( XRootDMsg, "[%s] VectorReader: read buffer for chunk %d@%ld",
                         url.GetHostId().c_str(), rdlst.rlen, rdlst.offset );

              //----------------------------------------------------------------
              // Mark chunk as done
              //----------------------------------------------------------------
              chstatus[chidx].done = true;

              //----------------------------------------------------------------
              // There is still data to be read, we need to readout the next
              // read list record.
              //----------------------------------------------------------------
              if( msgbtsrd < dlen )
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
              // Just drop the connection, we don't know if the stream is sane
              // anymore. Recover with a reconnect.
              return XRootDStatus( stError, errCorruptedHeader );
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
        //--------------------------------------------------------------------------
        // See if all the chunks are OK and put them in the response
        //--------------------------------------------------------------------------
        std::unique_ptr<VectorReadInfo> ptr( new VectorReadInfo() );
        for( uint32_t i = 0; i < chunks->size(); ++i )
        {
          if( !chstatus[i].done )
             return Status( stFatal, errInvalidResponse );
          ptr->GetChunks().emplace_back( ( *chunks )[i].offset,
              ( *chunks )[i].length, ( *chunks )[i].buffer );
        }
        ptr->SetSize( rawbtsrd );
        response = new AnyObject();
        response->Set( ptr.release() );
        return XRootDStatus();
      }

    private:

      size_t                    rdlstoff;     //< offset within the current read_list
      readahead_list            rdlst;        //< the readahead list for the current chunk
      size_t                    rdlstlen;     //< bytes left to be readout into read list
  };

} /* namespace XrdCl */

#endif /* SRC_XRDCL_XRDCLASYNCVECTORREADER_HH_ */
