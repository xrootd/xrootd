//------------------------------------------------------------------------------
// Copyright (c) 2011-2014 by European Organization for Nuclear Research (CERN)
// Author: Michal Simon <michal.simon@cern.ch>
//------------------------------------------------------------------------------
// This file is part of the XRootD software suite.
//
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
//
// In applying this licence, CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
//------------------------------------------------------------------------------

#ifndef SRC_XRDZIP_XRDZIPINFLCACHE_HH_
#define SRC_XRDZIP_XRDZIPINFLCACHE_HH_

#include "XrdCl/XrdClXRootDResponses.hh"
#include <zlib.h>
#include <exception>
#include <string>
#include <vector>
#include <mutex>
#include <queue>
#include <tuple>

namespace XrdCl
{
  //---------------------------------------------------------------------------
  //! An exception for carrying the XRootDStatus of InflCache
  //---------------------------------------------------------------------------
  struct ZipError : public std::exception
  {
      ZipError( const XrdCl::XRootDStatus &status ) : status( status )
      {
      }

      XrdCl::XRootDStatus status;
  };

  //---------------------------------------------------------------------------
  //! Utility class for inflating a compressed buffer
  //---------------------------------------------------------------------------
  class ZipCache
  {
    public:

      typedef std::vector<char> buffer_t;

    private:

      typedef std::tuple<uint64_t, uint32_t, void*, ResponseHandler*> read_args_t;
      typedef std::tuple<XRootDStatus, uint64_t, buffer_t> read_resp_t;

      struct greater_read_resp_t
      {
        inline bool operator() ( const read_resp_t &lhs, const read_resp_t &rhs ) const
        {
          return std::get<1>( lhs ) > std::get<1>( rhs );
        }
      };

      typedef std::priority_queue<read_resp_t, std::vector<read_resp_t>, greater_read_resp_t> resp_queue_t;

    public:

      ZipCache() : inabsoff( 0 )
      {
        strm.zalloc    = Z_NULL;
        strm.zfree     = Z_NULL;
        strm.opaque    = Z_NULL;
        strm.avail_in  = 0;
        strm.next_in   = Z_NULL;
        strm.avail_out = 0;
        strm.next_out  = Z_NULL;

        // make sure zlib doesn't look for gzip headers, in order to do so
        // pass negative window bits !!!
        int rc = inflateInit2( &strm, -MAX_WBITS );
        XrdCl::XRootDStatus st = ToXRootDStatus( rc, "inflateInit2" );
        if( !st.IsOK() ) throw ZipError( st );
      }

      ~ZipCache()
      {
        inflateEnd( &strm );
      }

      inline void QueueReq( uint64_t offset, uint32_t length, void *buffer, ResponseHandler *handler )
      {
        std::unique_lock<std::mutex> lck( mtx );
        rdreqs.emplace( offset, length, buffer, handler );
        Decompress();
      }

      inline void QueueRsp( const XRootDStatus &st, uint64_t offset, buffer_t &&buffer )
      {
        std::unique_lock<std::mutex> lck( mtx );
        rdrsps.emplace( st, offset, std::move( buffer ) );
        Decompress();
      }

    private:

      inline bool HasInput() const
      {
        return strm.avail_in != 0;
      }

      inline bool HasOutput() const
      {
        return strm.avail_out != 0;
      }

      inline void Input( const read_resp_t &rdrsp )
      {
        const buffer_t &buffer = std::get<2>( rdrsp );
        strm.avail_in = buffer.size();
        strm.next_in  = (Bytef*)buffer.data();
      }

      inline void Output( const read_args_t &rdreq )
      {
        strm.avail_out = std::get<1>( rdreq );
        strm.next_out  = (Bytef*)std::get<2>( rdreq );
      }

      inline bool Consecutive( const read_resp_t &resp ) const
      {
        return ( std::get<1>( resp ) == inabsoff );
      }

      void Decompress()
      {
        while( HasInput() || HasOutput() || !rdreqs.empty() || !rdrsps.empty() )
        {
          if( !HasOutput() && !rdreqs.empty() )
            Output( rdreqs.front() );

          if( !HasInput() && !rdrsps.empty() && Consecutive( rdrsps.top() ) ) // the response might come out of order so we need to check the offset
            Input( rdrsps.top() );

          if( !HasInput() || !HasOutput() ) return;

          // check the response status
          XRootDStatus st = std::get<0>( rdrsps.top() );
          if( !st.IsOK() ) return CallHandler( st );

          // the available space in output buffer before inflating
          uInt avail_before = strm.avail_in;
          // decompress the data
          int rc = inflate( &strm, Z_SYNC_FLUSH );
          st = ToXRootDStatus( rc, "inflate" );
          if( !st.IsOK() ) return CallHandler( st ); // report error to user handler
          // update the absolute input offset by the number of bytes we consumed
          inabsoff += avail_before - strm.avail_in;

          if( !strm.avail_out ) // the output buffer is empty meaning a request has been fulfilled
            CallHandler( XRootDStatus() );

          // the input buffer is empty meaning a response has been consumed
          // (we need to check if there are any elements in the responses
          // queue as the input buffer might have been set directly by the user)
          if( !strm.avail_in && !rdrsps.empty() )
            rdrsps.pop();
        }
      }

      static inline AnyObject* PkgRsp( ChunkInfo *chunk )
      {
        if( !chunk ) return nullptr;
        AnyObject *rsp = new AnyObject();
        rsp->Set( chunk );
        return rsp;
      }

      inline void CallHandler( const XRootDStatus &st )
      {
        if( rdreqs.empty() ) return;
        read_args_t args = std::move( rdreqs.front() );
        rdreqs.pop();

        ChunkInfo *chunk = nullptr;
        if( st.IsOK() ) chunk = new ChunkInfo( std::get<0>( args ),
                                                   std::get<1>( args ),
                                                   std::get<2>( args ) );

        ResponseHandler *handler = std::get<3>( args );
        handler->HandleResponse( new XRootDStatus( st ), PkgRsp( chunk ) );
      }

      XrdCl::XRootDStatus ToXRootDStatus( int rc, const std::string &func )
      {
        std::string msg = "[zlib] " + func + " : ";

        switch( rc )
        {
          case Z_STREAM_END    :
          case Z_OK            : return XrdCl::XRootDStatus();
          case Z_BUF_ERROR     : return XrdCl::XRootDStatus( XrdCl::stOK,    XrdCl::suContinue );
          case Z_MEM_ERROR     : return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInternal,    Z_MEM_ERROR,     msg + "not enough memory." );
          case Z_VERSION_ERROR : return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInternal,    Z_VERSION_ERROR, msg + "version mismatch." );
          case Z_STREAM_ERROR  : return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInvalidArgs, Z_STREAM_ERROR,  msg + "invalid argument." );
          case Z_NEED_DICT     : return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errDataError,   Z_NEED_DICT,     msg + "need dict.");
          case Z_DATA_ERROR    : return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errDataError,   Z_DATA_ERROR,    msg + "corrupted data." );
          default              : return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errUnknown );
        }
      }

      z_stream  strm;      // the zlib stream we will use for reading

      std::mutex              mtx;
      uint64_t                inabsoff; //< the absolute offset in the input file (compressed), ensures the user is actually streaming the data
      std::queue<read_args_t> rdreqs;   //< pending read requests  (we only allow read requests to be submitted in order)
      resp_queue_t            rdrsps;   //< pending read responses (due to multiple-streams the read response may come out of order)
  };

}

#endif /* SRC_XRDZIP_XRDZIPINFLCACHE_HH_ */
