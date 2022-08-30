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

#ifndef SRC_XRDCL_XRDCLASYNCPAGEREADER_HH_
#define SRC_XRDCL_XRDCLASYNCPAGEREADER_HH_

#include "XrdCl/XrdClXRootDResponses.hh"
#include "XrdCl/XrdClSocket.hh"
#include "XrdOuc/XrdOucPgrwUtils.hh"
#include "XrdSys/XrdSysPageSize.hh"

#include <sys/uio.h>
#include <memory>
#include <arpa/inet.h>

namespace XrdCl
{

//------------------------------------------------------------------------------
//! Object for reading out data from the PgRead response
//------------------------------------------------------------------------------
class AsyncPageReader
{
  public:

    //--------------------------------------------------------------------------
    //! Constructor
    //!
    //! @param chunks  : list of buffer for the data
    //! @param digests : a vector that will be filled with crc32c digest data
    //--------------------------------------------------------------------------
    AsyncPageReader( ChunkList             &chunks,
                     std::vector<uint32_t> &digests ) :
        chunks( chunks ),
        digests( digests ),
        dlen( 0 ),
        rspoff( 0 ),
        chindex( 0 ),
        choff( 0 ),
        dgindex( 0 ),
        dgoff( 0 ),
        iovcnt( 0 ),
        iovindex( 0 )
    {
      uint64_t rdoff = chunks.front().offset;
      uint32_t rdlen = 0;
      for( auto &ch : chunks )
        rdlen += ch.length;
      int fpglen, lpglen;
      int pgcnt = XrdOucPgrwUtils::csNum( rdoff, rdlen, fpglen, lpglen);
      digests.resize( pgcnt );
    }

    //--------------------------------------------------------------------------
    //! Destructor
    //--------------------------------------------------------------------------
    virtual ~AsyncPageReader()
    {
    }

    //--------------------------------------------------------------------------
    //! Sets message data size
    //--------------------------------------------------------------------------
    void SetRsp( ServerResponseV2 *rsp )
    {
      dlen = rsp->status.bdy.dlen;
      rspoff = rsp->info.pgread.offset;

      uint64_t bufoff = rspoff - chunks[0].offset;
      chindex = 0;

      for( chindex = 0; chindex < chunks.size(); ++chindex )
      {
        if( chunks[chindex].length < bufoff )
        {
          bufoff -= chunks[chindex].length;
          continue;
        }
        break;
      }
      choff = bufoff;
    }

    //--------------------------------------------------------------------------
    //! Readout data from the socket
    //! @param socket  : the socket with the data 
    //! @param btsread : number of user data read from the socket
    //! @return        : operation status
    //--------------------------------------------------------------------------
    XRootDStatus Read( Socket &socket, uint32_t &btsread )
    {
      if( dlen == 0 || chindex >= chunks.size() )
        return XRootDStatus();
      btsread = 0;
      int nbbts = 0;
      do
      {
        // Prepare the IO vector for receiving the data
        if( iov.empty() )
          InitIOV();
        // read the data into the buffer
        nbbts = 0;
        auto st = socket.ReadV( iov.data() + iovindex, iovcnt, nbbts );
        if( !st.IsOK() )
          return st;
        btsread += nbbts;
        dlen    -= nbbts;
        ShiftIOV( nbbts );
        if( st.code == suRetry )
          return st;
      }
      while( nbbts > 0 && dlen > 0 && chindex < chunks.size() );

      return XRootDStatus();
    }

  private:

    //--------------------------------------------------------------------------
    //! Helper class for retrieving the maximum size of the I/O vector
    //--------------------------------------------------------------------------
    struct iovmax_t
    {
      iovmax_t()
      {
#ifdef _SC_IOV_MAX
        value = sysconf(_SC_IOV_MAX);
        if (value == -1)
#endif
#ifdef IOV_MAX
          value = IOV_MAX;
#else
          value = 1024;
#endif
        value &= ~uint32_t( 1 ); // make sure it is an even number
      }
      int32_t value;
    };

    //--------------------------------------------------------------------------
    //! @return : maximum size of I/O vector
    //--------------------------------------------------------------------------
    inline static int max_iovcnt()
    {
      static iovmax_t iovmax;
      return iovmax.value;
    }

    //--------------------------------------------------------------------------
    //! Add I/O buffer to the vector
    //--------------------------------------------------------------------------
    inline void addiov( char *&buf, size_t len )
    {
      iov.emplace_back();
      iov.back().iov_base = buf;
      iov.back().iov_len  = len;
      buf += len;
      ++iovcnt;
    }

    //--------------------------------------------------------------------------
    //! Add I/O buffer to the vector and update number of bytes left to be read
    //--------------------------------------------------------------------------
    inline void addiov( char *&buf, uint32_t len, uint32_t &dleft )
    {
      if( len > dleft ) len = dleft;
      addiov( buf, len );
      dleft -= len;
    }

    //--------------------------------------------------------------------------
    //! Calculate the size of the I/O vector
    //! @param dleft : data to be accomodated by the I/O vector
    //--------------------------------------------------------------------------
    inline static uint32_t CalcIOVSize( uint32_t dleft )
    {
      uint32_t ret = ( dleft / PageWithDigest + 2 ) * 2;
      return ( ret > uint32_t( max_iovcnt() ) ? max_iovcnt() : ret );
    }

    //--------------------------------------------------------------------------
    //! Calculate the size of the data to be read
    //--------------------------------------------------------------------------
    uint32_t CalcRdSize()
    {
      // data size in the server response (including digests)
      uint32_t dleft = dlen;
      // space in our page buffer
      uint32_t pgspace = chunks[chindex].length - choff;
      // space in our digest buffer
      uint32_t dgspace = sizeof( uint32_t ) * (digests.size() - dgindex ) - dgoff;
      if( dleft > pgspace + dgspace )
        dleft = pgspace + dgspace;
      return dleft;
    }

    //--------------------------------------------------------------------------
    //! Initialize the I/O vector
    //--------------------------------------------------------------------------
    void InitIOV()
    {
      iovindex = 0;
      // figure out the number of data we can read in one go
      uint32_t dleft = CalcRdSize();
      // and reset the I/O vector
      iov.clear();
      iovcnt = 0;
      iov.reserve( CalcIOVSize( dleft ) );
      // now prepare the page and digest buffers
      ChunkInfo ch    = chunks[chindex];
      char*     pgbuf = static_cast<char*>( ch.buffer ) + choff;
      uint64_t  rdoff = ch.offset + choff;
      char*     dgbuf = reinterpret_cast<char*>( digests.data() + dgindex ) + dgoff;
      // handle the first digest
      uint32_t fdglen = sizeof( uint32_t ) - dgoff;
      addiov( dgbuf, fdglen, dleft );
      if( dleft == 0 || iovcnt >= max_iovcnt() )
        return;
      // handle the first page
      uint32_t fpglen = XrdSys::PageSize - rdoff % XrdSys::PageSize;
      addiov( pgbuf, fpglen, dleft );
      if( dleft == 0 || iovcnt >= max_iovcnt() )
        return;
      // handle all the subsequent aligned pages
      size_t fullpgs = dleft / PageWithDigest;
      for( size_t i = 0; i < fullpgs; ++i )
      {
        addiov( dgbuf, sizeof( uint32_t ), dleft );
        if( dleft == 0 || iovcnt >= max_iovcnt() )
          return;
        addiov( pgbuf, XrdSys::PageSize, dleft );
        if( dleft == 0 || iovcnt >= max_iovcnt() )
          return;
      }
      // handle the last digest
      uint32_t ldglen = sizeof( uint32_t );
      addiov( dgbuf, ldglen, dleft );
      if( dleft == 0 || iovcnt >= max_iovcnt() )
        return;
      // handle the last page
      addiov( pgbuf, dleft, dleft );
    }

    //--------------------------------------------------------------------------
    //! Shift buffer by a number of bytes
    //--------------------------------------------------------------------------
    inline void shift( void *&buffer, size_t nbbts )
    {
      char *buf = static_cast<char*>( buffer );
      buf += nbbts;
      buffer = buf;
    }

    //--------------------------------------------------------------------------
    //! shift digest buffer by `btsread`
    //! @param btsread : total number of bytes read (will be decremented by bytes
    //!                  shifted in buffer)
    //--------------------------------------------------------------------------
    inline void shiftdgbuf( uint32_t &btsread )
    {
      if( iov[iovindex].iov_len > btsread )
      {
        iov[iovindex].iov_len -= btsread;
        shift( iov[iovindex].iov_base, btsread );
        dgoff += btsread;
        btsread = 0;
        return;
      }

      btsread -= iov[iovindex].iov_len;
      iov[iovindex].iov_len = 0;
      dgoff = 0;
      digests[dgindex] = ntohl( digests[dgindex] );
      ++dgindex;
      ++iovindex;
      --iovcnt;
    }

    //--------------------------------------------------------------------------
    //! shift page buffer by `btsread`
    //! @param btsread : total number of bytes read (will be decremented by bytes
    //!                  shifted in buffer)
    //--------------------------------------------------------------------------
    inline void shiftpgbuf( uint32_t &btsread )
    {
      if( iov[iovindex].iov_len > btsread )
      {
        iov[iovindex].iov_len -= btsread;
        shift( iov[iovindex].iov_base, btsread );
        choff += btsread;
        btsread = 0;
        return;
      }

      btsread -= iov[iovindex].iov_len;
      choff   += iov[iovindex].iov_len;
      iov[iovindex].iov_len = 0;
      ++iovindex;
      --iovcnt;
    }

    //--------------------------------------------------------------------------
    //! shift the I/O vector by the number of bytes read
    //--------------------------------------------------------------------------
    void ShiftIOV( uint32_t btsread )
    {
      // if iovindex is even it point to digest, otherwise it points to a page
      if( iovindex % 2 == 0 )
        shiftdgbuf( btsread );
      // adjust as many I/O buffers as necessary
      while( btsread > 0 )
      {
        // handle page
        shiftpgbuf( btsread );
        if( btsread == 0 ) break;
        // handle digest
        shiftdgbuf( btsread );
      }
      // if we filled the buffer, move to the next one
      if( iovcnt == 0 )
        iov.clear();
      // do we need to move to the next chunk?
      if( choff >= chunks[chindex].length )
      {
        ++chindex;
        choff = 0;
      }
    }

    ChunkList &chunks;              //< list of data chunks to be filled with user data
    std::vector<uint32_t> &digests; //< list of crc32c digests for every 4KB page of data
    uint32_t   dlen;                //< size of the data in the message
    uint64_t   rspoff;              //< response offset

    size_t chindex;                 //< index of the current data buffer
    size_t choff;                   //< offset within the current buffer
    size_t dgindex;                 //< index of the current digest buffer
    size_t dgoff;                   //< offset within the current digest buffer

    std::vector<iovec> iov;         //< I/O vector
    int                iovcnt;      //< size of the I/O vector
    size_t             iovindex;    //< index of the first valid element in the I/O vector

    static const int PageWithDigest = XrdSys::PageSize + sizeof( uint32_t );
};

} /* namespace XrdEc */

#endif /* SRC_XRDCL_XRDCLASYNCPAGEREADER_HH_ */
