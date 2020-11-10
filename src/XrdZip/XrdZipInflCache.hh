/*
 * XrdZipInfltCache.hh
 *
 *  Created on: 10 Nov 2020
 *      Author: simonm
 */

#ifndef SRC_XRDZIP_XRDZIPINFLCACHE_HH_
#define SRC_XRDZIP_XRDZIPINFLCACHE_HH_

#include "XrdCl/XrdClXRootDResponses.hh"
#include <zlib.h>
#include <exception>
#include <string>

namespace XrdZip
{
  //---------------------------------------------------------------------------
  //! An exception for carrying the XRootDStatus of InflCache
  //---------------------------------------------------------------------------
  struct InflCacheError : public std::exception
  {
      InflCacheError( const XrdCl::XRootDStatus &status ) : status( status )
      {
      }

      XrdCl::XRootDStatus status;
  };

  //---------------------------------------------------------------------------
  //! Utility class for inflating a compressed buffer
  //---------------------------------------------------------------------------
  class InflCache
  {
    public:

      InflCache() : rawOffset( 0 ), rawSize( 0 ), totalRead( 0 )
      {
        strm.zalloc   = Z_NULL;
        strm.zfree    = Z_NULL;
        strm.opaque   = Z_NULL;
        strm.avail_in = 0;
        strm.next_in  = Z_NULL;

        // make sure zlib doesn't look for gzip headers, in order to do so
        // pass negative window bits !!!
        int rc = inflateInit2( &strm, -MAX_WBITS );
        XrdCl::XRootDStatus st = ToXRootDStatus( rc, "inflateInit2" );
        if( !st.IsOK() ) throw InflCacheError( st );
      }

      ~InflCache()
      {
        inflateEnd( &strm );
      }

      XrdCl::XRootDStatus Input( void *inbuff, size_t insize, uint64_t rawoff )
      {
        // we only support streaming for compressed files
        if( rawoff != rawOffset + rawSize )
          return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInternal );

        strm.avail_in = insize;
        strm.next_in  = (Bytef*)inbuff;
        rawOffset     = rawoff;
        rawSize       = insize;

        return XrdCl::XRootDStatus();
      }

      XrdCl::XRootDStatus Output( void *outbuff, size_t outsize, uint64_t offset )
      {
        // we only support streaming for compressed files
        if( offset != totalRead )
          return XrdCl::XRootDStatus( XrdCl::stError, XrdCl::errInternal );

        strm.avail_out = outsize;
        strm.next_out  = (Bytef*)outbuff;

        return XrdCl::XRootDStatus();
      }

      XrdCl::XRootDStatus Read( uint32_t &bytesRead )
      {
        // the available space in output buffer before inflating
        uInt avail_before = strm.avail_out;

        int rc = inflate( &strm, Z_SYNC_FLUSH );
        XrdCl::XRootDStatus st = ToXRootDStatus( rc, "inflate" );
        if( !st.IsOK() ) return st;

        bytesRead = avail_before - strm.avail_out;
        totalRead += bytesRead;
        if( strm.avail_out ) return XrdCl::XRootDStatus( XrdCl::stOK, XrdCl::suPartial );

        return XrdCl::XRootDStatus();
      }

      uint64_t NextChunkOffset()
      {
        return rawOffset + rawSize;
      }

    private:

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

      uint64_t  rawOffset; // offset of the raw data chunk in the compressed file (not archive)
      uint32_t  rawSize;   // size of the raw data chunk
      uint64_t  totalRead; // total number of bytes read so far
      z_stream  strm;      // the zlib stream we will use for reading
  };

}

#endif /* SRC_XRDZIP_XRDZIPINFLCACHE_HH_ */
