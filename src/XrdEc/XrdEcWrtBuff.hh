/*
 * XrdEcWrtBuff.hh
 *
 *  Created on: Oct 14, 2019
 *      Author: simonm
 */

#ifndef SRC_XRDEC_XRDECWRTBUFF_HH_
#define SRC_XRDEC_XRDECWRTBUFF_HH_

#include "XrdEc/XrdEcUtilities.hh"
#include "XrdEc/XrdEcScheduleHandler.hh"
#include "XrdEc/XrdEcObjCfg.hh"
#include "XrdEc/XrdEcConfig.hh"
#include "XrdEc/XrdEcThreadPool.hh"

#include "XrdCl/XrdClBuffer.hh"
#include "XrdCl/XrdClXRootDResponses.hh"

#include "XrdOuc/XrdOucCRC32C.hh"

#include <utility>
#include <vector>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <future>

namespace XrdEc
{
  class BufferPool
  {
    public:

      static BufferPool& Instance()
      {
        static BufferPool instance;
        return instance;
      }

      XrdCl::Buffer Create( const ObjCfg &objcfg )
      {
        std::unique_lock<std::mutex> lck( mtx );

        if( !pool.empty() )
        {
          XrdCl::Buffer buffer( std::move( pool.front() ) );
          pool.pop();
          return std::move( buffer );
        }

        if( currentsize < totalsize )
        {
          XrdCl::Buffer buffer( objcfg.blksize );
          ++currentsize;
          return std::move( buffer );
        }

        while( pool.empty() ) cv.wait( lck );

        XrdCl::Buffer buffer( std::move( pool.front() ) );
        pool.pop();
        return std::move( buffer );
      }

      void Recycle( XrdCl::Buffer && buffer )
      {
        if( !buffer.GetBuffer() ) return;
        std::unique_lock<std::mutex> lck( mtx );
        buffer.SetCursor( 0 );
        pool.emplace( std::move( buffer ) );
        cv.notify_all();
      }

    private:

      BufferPool() : totalsize( 1024 ), currentsize( 0 )
      {
      }


      BufferPool( const BufferPool& ) = delete;
      BufferPool( BufferPool&& ) = delete;

      BufferPool& operator=( const BufferPool& ) = delete;
      BufferPool& operator=( BufferPool&& ) = delete;

      const size_t               totalsize;
      size_t                     currentsize;
      std::condition_variable    cv;
      std::mutex                 mtx;
      std::queue<XrdCl::Buffer>  pool;
  };

  class WrtBuff
  {
    public:

      WrtBuff( const ObjCfg &objcfg ) : objcfg( objcfg ), wrtbuff( BufferPool::Instance().Create( objcfg ) )
      {
        stripes.reserve( objcfg.nbchunks );
        memset( wrtbuff.GetBuffer(), 0, wrtbuff.GetSize() );
      }

      WrtBuff( WrtBuff && wrtbuff ) : objcfg( wrtbuff.objcfg ),
                                      wrtbuff( std::move( wrtbuff.wrtbuff ) ),
                                      stripes( std::move( wrtbuff.stripes ) )
      {
      }

      ~WrtBuff()
      {
        BufferPool::Instance().Recycle( std::move( wrtbuff ) );
      }

      uint32_t Write( uint32_t size, const char *buffer )
      {
        uint64_t bytesAccepted = size;
        if( wrtbuff.GetCursor() + bytesAccepted > objcfg.datasize )
          bytesAccepted = objcfg.datasize - wrtbuff.GetCursor();
        memcpy( wrtbuff.GetBufferAtCursor(), buffer, bytesAccepted );
        wrtbuff.AdvanceCursor( bytesAccepted );

        return bytesAccepted;
      }

      void Pad( uint32_t size )
      {
        // if the buffer exist we only need to move the cursor
        if( wrtbuff.GetSize() != 0 )
        {
          wrtbuff.AdvanceCursor( size );
          return;
        }

        // otherwise we allocate the buffer and set the cursor
        wrtbuff.Allocate( objcfg.datasize );
        memset( wrtbuff.GetBuffer(), 0, wrtbuff.GetSize() );
        wrtbuff.SetCursor( size );
        return;
      }

      inline char* GetStrpBuff( uint8_t strpnb )
      {
        return stripes[strpnb].buffer;
      }

      uint32_t GetStrpSize( uint8_t strp )
      {
        // Check if it is a data chunk?
        if( strp < objcfg.nbdata )
        {
          // If the cursor is at least at the expected size
          // it means we have the full chunk.
          uint64_t expsize = ( strp + 1) * objcfg.chunksize;
          if( expsize <= wrtbuff.GetCursor() )
            return objcfg.chunksize;

          // If the cursor is of by less than the chunk size
          // it means we have a partial chunk
          uint64_t delta = expsize - wrtbuff.GetCursor();
          if( delta < objcfg.chunksize )
            return objcfg.chunksize - delta;

          // otherwise we are handling an empty chunk
          return 0;
        }

        // It is a parity chunk so its size has  to be equal
        // to the size of the first chunk
        return GetStrpSize( 0 );
      }

      inline uint32_t GetBlkSize()
      {
        return wrtbuff.GetCursor();
      }

      inline bool Complete()
      {
        return wrtbuff.GetCursor() == objcfg.datasize;
      }

      inline bool Empty()
      {
        return ( wrtbuff.GetSize() == 0 || wrtbuff.GetCursor() == 0 );
      }

      inline void Encode()
      {
        // first calculate the parity
        uint8_t i ;
        for( i = 0; i < objcfg.nbchunks; ++i )
          stripes.emplace_back( wrtbuff.GetBuffer( i * objcfg.chunksize ), i < objcfg.nbdata );
        Config &cfg = Config::Instance();
        cfg.GetRedundancy( objcfg ).compute( stripes );

        // then calculate the checksums
        cksums.reserve( objcfg.nbchunks );
        for( uint8_t strpnb = 0; strpnb < objcfg.nbchunks; ++strpnb )
        {
          std::future<uint32_t> ftr = ThreadPool::Instance().Execute( crc32c, 0, stripes[strpnb].buffer, objcfg.chunksize );
          cksums.emplace_back( std::move( ftr ) );
        }
      }

      inline uint32_t GetCrc32c( size_t strpnb )
      {
        return cksums[strpnb].get();
      }

    private:

      ObjCfg                             objcfg;
      XrdCl::Buffer                      wrtbuff;
      stripes_t                          stripes;
      std::vector<std::future<uint32_t>> cksums;
  };


} /* namespace XrdEc */

#endif /* SRC_XRDEC_XRDECWRTBUFF_HH_ */
